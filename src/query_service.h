#pragma once

#include <fmt/core.h>
#include <folly/MPMCQueue.h>
#include <folly/Synchronized.h>

#include <algorithm>
#include <functional>
#include <stdexcept>

#include "types.h"
#include "utils/logging.h"

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

    // fmt::println("{} {}, {} {}", a_s, a_e, b_s, b_e);

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

std::string formatResult(const json& obj) {
    std::string out;
    for (auto it = obj.items().begin(); it != obj.items().end(); ++it) {
        if (!out.empty()) {
            out += ",  ";
        }
        fmt::format_to(
            std::back_inserter(out), "{}: {}", it.key(), it.value().dump()
        );
    }
    return out;
}

std::optional<QueryResult> runQueryOnIndex(Index& index, Query&& query) {
    std::vector<std::string> formattedLines;  // return type
    json                     filtered;        // cache to reduce allocations

    // iterate over valid jsonLines indices
    BitSet     filter = linesWithPathRoot(index, query);
    const auto rend   = filter.rend();
    for (auto it = filter.rbegin(); it != rend; ++it) {
        if (formattedLines.size() == query.maxMatches) {
            break;
        }

        const json& jsonLine = index.lines[*it];

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
        return std::nullopt;
    }

    return QueryResult(std::move(query), std::move(formattedLines));
}

void startQueryService(
    folly::MPMCQueue<Msg>&            rx,
    folly::Synchronized<QueryResult>& queryResult,
    std::function<void()>             onResult
) {
    auto info = [tag = json{{"tag", "QS"}
                 }](std::string&& s, std::optional<json> obj = std::nullopt) {
        if (obj) {
            Log::info(s, Log::merge(std::move(*obj), tag));
        }
        Log::info(s, tag);
    };

    info("Starting query service");
    Index index;
    Msg   msg;
    auto  handleQuery = [&](Query&& query) {
        auto result = runQueryOnIndex(index, std::move(query));
        info("runQueryOnIndex returned");
        if (result) {
            info("result is Some");
            *queryResult.wlock() = std::move(*result);
        }
        info("Calling onResult");
        onResult();
    };

    for (bool shouldContinue = true; shouldContinue;) {
        rx.blockingRead(msg);
        std::visit(
            overloaded{
                [&](StopSignal&) { shouldContinue = false; },
                [&](Query& query) {
                    info("Msg::Query: Start");
                    handleQuery(std::move(query));
                    info("Msg::Query: End\n");
                },
                [&](Index& update) {
                    info("Msg::Index: Start");
                    mergeIndex(index, update);
                    // re-run last query on updated index
                    Query query;
                    { query = queryResult.rlock()->query.clone(); }
                    if (query.seq == 0) {
                        info("Msg::Index: End\n");
                        return;
                    }
                    handleQuery(std::move(query));
                    info("Msg::Index: End\n");
                },
            },
            msg
        );
    }
}

std::thread spawnQueryService(
    folly::MPMCQueue<Msg>&            rx,
    folly::Synchronized<QueryResult>& queryResult,
    std::function<void()>&            onUpdate
) {
    return std::thread([&]() { startQueryService(rx, queryResult, onUpdate); });
}
