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
#include "lib.h"
#include "logging.h"
#include "parser.h"
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
// - [X] More recent log appears at bottom
// - [ ] Stream new changes to file
// - [ ] Fix lag

// TODO: Build in-memory file repr that is fast to search
// -

int main(int argc, char** argv) {
    Log::init("log.json");
    Log::sendToCout = false;
    Log::info("Hello from Live Log Query (llq)!");

    std::string fname = argc > 1 ? argv[1] : "dummy_log.json";

    folly::Synchronized<QueryResult> queryResult;
    folly::MPMCQueue<Msg>            channel;

    std::ifstream file(fname, std::ifstream::in);

    startIngesting(channel, file);
    auto onUpdate = ui(channel, queryResult);
    startQueryService(channel, queryResult, onUpdate);
}
