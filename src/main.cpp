#include <atomic>
#define DOCTEST_CONFIG_DISABLE
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <doctest.h>
#include <fmt/core.h>
#include <folly/MPMCQueue.h>
#include <folly/Synchronized.h>

#include <fstream>
#include <string>  // for char_traits, operator+, string, basic_string
#include <thread>

#include "ingestor.h"
#include "logging.h"
#include "query_service.h"
#include "ui.h"

/*
 * main -> glue code
 *
 * ingestor -> file reading, Index creation
 * query_service -> query service,
 * types -> Index, Query, QueryResult, Msg defintions
 * ui -> ftxui
 * parser -> parsing
 * logging -> logging
 *
 */

/*
 * Index: structure containing
 * - start_idx: int // the line number from the log file this index begins with;
 * used for partial indexes
 * - lines: Vec<json>
 * - bitsets: HashMap<json_pointer, Bitset>
 * - Merge function updates first Index to include 2nd Index,
 *     Note: start_idx + lines.size() ranges must be overlapping or adjacent so
 * final range is contiguous
 *
 * Ingestor: Continuously reads lines from file.
 * When it finds a new line:
 * - parse line into json
 * - update bitset for each key in line
 * - push json line into vec of lines
 *
 *
 * QueryService: Takes list of Expr and finds set of lines that matches
 * - Maintains Index
 * - Reads from in-channel either Query or Index msgs
 *   - On Index: merge incoming w/ master index
 *   - On Query: run query on master index
 *     - && together bitsets for all paths in query to make a single bitset
 * filter
 *     - Iterate `lines` and apply any filter ops from query, then format output
 * and return
 *
 * UI Thread:
 * - If input parses, send to query service along with callback to run on
 * completion
 * - Callback schedules task on UI thread which updates local variables and
 * posts a custom event to cause re-render
 *
 *
 */

// TODO:
// - [ ] Make scrollable lines work
// - [ ] Stream new changes to file
// - [ ] Fix lag

// TODO: Build in-memory file repr that is fast to search
// -

int main(int argc, char** argv) {
    Log::init("log.json");
    Log::sendToCout = false;
    Log::info("Hello from Live Log Query (llq)!");

    auto info = [tag = json{{"tag", "Main"}}](const std::string&& s) {
        Log::info(s, tag);
    };

    // TODO: think about correct number here
    folly::MPMCQueue<Msg> channel(100);

    // spawn ingestor to listen to log file
    std::atomic<bool> shouldShutdown(false);
    std::string       fname = argc > 1 ? argv[1] : "dummy_log.json";
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
