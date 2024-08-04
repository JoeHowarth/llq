#pragma once

#include <fmt/core.h>
#include <folly/MPMCQueue.h>
#include <folly/Synchronized.h>

#include <algorithm>
#include <functional>
#include <stdexcept>

#include "lib.h"
#include "types.h"

// helper type for the visitor
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

bool mergeIndex(Index& index, Index& other, bool throw_on_gap = true) {
    // assumption:
    assert(index.start_idx <= other.start_idx);

    const auto a_s     = index.start_idx;
    const auto a_e     = a_s + index.lines.size() - 1;
    const auto b_s     = other.start_idx;
    const auto b_e     = b_s + other.lines.size() - 1;
    const bool a_gap_b = b_s > a_e + 1;
    if (a_gap_b) {
        // can't merge if combining ranges results in a gap (i.e. result must be
        // contiguous)
        if (throw_on_gap) {
            throw std::runtime_error(fmt::format(
                "Merging incoming index results in a gap. a_e: {}, b_s: {}",
                a_e, b_s
            ));
        }
        return false;
    }

    fmt::println("{} {}, {} {}", a_s, a_e, b_s, b_e);

    const auto b_start_idx      = a_e + 1 - b_s;
    const auto a_idx_from_b_idx = [=](std::size_t b_idx) {
        return b_idx + b_s;
    };

    for (auto b_idx = b_start_idx; b_idx < other.lines.size(); ++b_idx) {
        index.lines.push_back(std::move(other.lines[b_idx]));
    }

    for (const auto& [k, other_bitset] : other.bitsets) {
        BitSet& bitset = index.bitsets[k];
        for (std::size_t i = a_e + 1 - b_s; i < other_bitset.size(); ++i) {
            bitset.set(a_idx_from_b_idx(i), other_bitset[i]);
        }
    }
    return true;
}

bool queryMatches(const Query& query, const json& line) {
    return std::ranges::all_of(query.exprs, [&](auto expr) {
        return expr.matches(line);
    });
}

BitSet linesWithPathRoot(const Index& index, const Query& query) {
    // and (&) together bitsets to find indices of lines that have all the roots
    // of paths in the expr
    BitSet filter = BitSet::trueMask(index.lines.size());
    for (const Expr& expr : query.exprs) {
        if (expr.path.isWildCard ||
            !index.bitsets.contains(expr.path.frontHash)) {
            continue;
        }
        filter &= index.bitsets.at(expr.path.frontHash);
    }
    return std::move(filter);
}

void runQuery(
    Index&                            index,
    folly::Synchronized<QueryResult>& queryResult,
    std::function<void()>&            onUpdate,
    Query&&                           query
) {
    BitSet                   filter = linesWithPathRoot(index, query);
    std::vector<std::string> formattedLines;
    json                     filtered;

    fmt::println("filter: {}", filter);

    // iterate over valid jsonLines indices
    const auto rend = filter.rend();
    for (auto it = filter.rbegin(); it != rend; ++it) {
        if (formattedLines.size() == query.maxMatches) {
            break;
        }

        const std::size_t idx      = *it;
        const json&       jsonLine = index.lines[idx];

        // ensure query matches before copying results into `filtered`
        if (!queryMatches(query, jsonLine)) {
            continue;
        }

        filtered.clear();  // clear from previous iteration
        for (const Expr& expr : query.exprs) {
            // TODO: Determine if this is really inefficient
            filtered[expr.path.ptr] = jsonLine[expr.path.ptr];
        }
        formattedLines.push_back(std::move(formatResult(filtered)));
    }

    // if query resulted in no matches, do not update query result
    if (formattedLines.size() == 0) {
        return;
    }

    // acquire write-lock and update the query result
    {
        auto qr   = queryResult.wlock();
        qr->query = std::move(query);
        qr->lines = std::move(formattedLines);
    }
    // signal that query returned successfully
    onUpdate();
}

void startQueryService(
    folly::MPMCQueue<Msg>&            rx,
    folly::Synchronized<QueryResult>& queryResult,
    std::function<void()>&            onUpdate
) {
    Index index;
    Msg   msg;
    while (true) {
        rx.blockingRead(msg);
        std::visit(
            overloaded{
                [&](Index& update) { mergeIndex(index, update); },
                [&](Query& query) {
                    runQuery(index, queryResult, onUpdate, std::move(query));
                }
            },
            msg
        );
    }
}

/*
void queryService(
    ftxui::ScreenInteractive& screen,
    ftxui::Receiver<Query>    rcvr,
    QueryResult&              queryResult,
    const std::string&        fname
) {
    auto sender = rcvr->MakeSender();
    // std::thread notifier([&queryResult, sender = std::move(sender)]() {
    //     while (true) {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(300));
    //         if (queryResult.query.str.size() > 0) {
    //             sender->Send(queryResult.query);
    //         }
    //     }
    // });

    int  maxMatches  = -1;
    long lastSeenSeq = 0;
    // TODO: screen.dimy() is likely not thread safe to read AND not
    // initialized correctly. This hack reads it from the main thread hopefully
    // after initialization. Replace with a better solution
    screen.Post([&maxMatches, &screen]() { maxMatches = screen.dimy() - 3; });
    Query query;
    while (rcvr->Receive(&query)) {
        if (lastSeenSeq > query.seq) {
            Log::info(
                "Query has old seq",
                {{"seq", query.seq}, {"lastSeen", lastSeenSeq}}
            );
            continue;
        }

        if (maxMatches == -1) {
            // FIXME: if still not initialized, pray
            maxMatches = screen.dimy() - 3;
        }

        std::vector<json> results =
            runQuery(query.exprs, fname, maxMatches, 10000);
        if (results.size() > 0) {
            // guard against `query` variable being overwritten before task runs
            // on ui thread
            std::string                queryStr = query.str;
            auto                       lines    = formatResults(results);
            std::optional<QueryResult> toMove(
                std::in_place, std::move(query), std::move(results),
                std::move(lines)
            );

            auto f = [moved = std::move(toMove)]() {
                std::cout << moved->lines[0] << std::endl;
            };

            screen.Post(f);
            // screen.Post([&screen, &queryResult, moved = std::move(toMove),
            //              &maxMatches]() mutable {
            //     queryResult = std::move(*moved);
            //     moved.reset();
            //     // update maxMatches to account for resizes
            //     maxMatches = screen.dimy() - 3;
            //
            //     screen.PostEvent(ftxui::Event::Custom);
            // });
        }
    }
}
*/
