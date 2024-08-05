#define DOCTEST_CONFIG_DISABLE
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <doctest.h>
#include <fmt/core.h>
#include <folly/MPMCQueue.h>
#include <folly/Synchronized.h>

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

#include "ingestor.h"
#include "logging.h"
#include "query_service.h"
#include "ui.h"

/*
 * Index: structure containing
 * - start_idx: int // the line number from the log file this index begins with;
 * used for partial indexes
 * - lines: Vec<json>
 * - bitsets: HashMap<"Hash of first key in json pointer", Bitset>
 * - Merge function updates first Index to include 2nd Index,
 *     Note: start_idx + lines.size() ranges must be overlapping or adjacent so
 *     final range is contiguous
 *
 * Ingestor: Continuously reads lines from file.
 * When it finds a new line:
 * - parse line into json
 * - Update Index:
 *   - update bitset for each key in line
 *   - push json line into vec of lines
 * - When no more lines left to read, send partial Index through output channel
 * (to QueryService)
 * - Sleep then check for more lines in file
 *
 * QueryService: Responsible for maaintaining full Index and running queries
 * against it
 * - Reads from in-channel either Query or Index msgs
 *   - On Index: merge incoming w/ master index, then re-run last query with new
 * Index
 *   - On Query: run query on master index
 *     - && together bitsets for all paths in query to make a single bitset
 *       filter
 *     - Iterate `lines` and apply any filter ops from query, then format output
 *     - Update QueryResult shared state, call onResult cb to trigger ftxui
 * re-render
 *
 * UI Thread:
 * - If input parses, send to query service
 * - On render, read from shared QueryResult to populate ui
 * - If Query is invalid or returns empty result, continue showing last
 * non-empty query
 */

// TODO: scrollable results

int main(int argc, char** argv) {
    if (argc < 2) {
        fmt::println(
            "LLQ (Live Log Query)\nInvalid usage, must specify a log file\n\nExample :> llq log.json"
        );
        exit(1);
    }
    std::string fname = argv[1];

    Log::disable();
    // Log::init("log.json");
    // Log::sendToCout = false;
    Log::info("Hello from Live Log Query (llq)!");

    // TODO: Generalize this tagged logger in `logging.h`
    auto info = [tag = json{{"tag", "Main"}}](const std::string&& s) {
        Log::info(s, tag);
    };

    // TODO: think about correct number here
    folly::MPMCQueue<Msg> channel(100);

    // spawn ingestor to listen to log file
    std::atomic<bool> shouldShutdown(false);
    std::ifstream     file(fname, std::ifstream::in);
    std::thread       ingestor = spawnIngestor(channel, file, shouldShutdown);

    // create onResult callback to re-render ftxui after successful query
    // evaluation
    auto                  screen = ftxui::ScreenInteractive::Fullscreen();
    std::function<void()> threadSafeReRender = [&screen]() {
        screen.Post([&screen]() { screen.PostEvent(ftxui::Event::Custom); });
    };

    // spawn query service
    folly::Synchronized<QueryResult> queryResult;
    std::thread                      queryService =
        spawnQueryService(channel, queryResult, threadSafeReRender);

    // run ui
    ui(screen, channel, queryResult);

    // shutdown
    {
        info("Shutting down workers...");
        shouldShutdown.store(true);
        channel.blockingWrite(StopSignal{});
        ingestor.join();
        queryService.join();
        info("Workers shutdown");
    }
}
