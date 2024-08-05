#pragma once

#include <vector>

#include "utils/bitset.h"
#include "expr.h"
#include "parser.h"

struct Index {
    using PathHash = std::size_t;

    std::size_t                          start_idx{};
    std::vector<json>                    lines;
    std::unordered_map<PathHash, BitSet> bitsets;

    Index() = default;

    Index(
        std::size_t                          s,
        std::vector<json>                    l,
        std::unordered_map<PathHash, BitSet> b
    )
        : start_idx(s), lines(std::move(l)), bitsets(std::move(b)) {}

    // Move constructor (noexcept)
    Index(Index&& other) noexcept
        : start_idx(other.start_idx)
        , lines(std::move(other.lines))
        , bitsets(std::move(other.bitsets)) {}  // FIXME:

    // Move assignment operator (noexcept)
    Index& operator=(Index&& other) noexcept {
        if (this != &other) {
            start_idx = other.start_idx;
            lines     = std::move(other.lines);
            bitsets   = std::move(other.bitsets);  // FIXME:
        }
        return *this;
    }
};

struct Query {
    long              seq = 0;
    std::string       str;
    std::vector<Expr> exprs;
    int               maxMatches = 1000;

    static std::optional<Query>
    parse(std::string str, long seq = 0, int maxMatches = 1000) {
        std::optional<std::vector<Expr>> exprs = parser::parseExprs(str);
        if (!exprs) {
            return std::nullopt;
        }
        return Query(seq, std::move(str), std::move(*exprs), maxMatches);
    }

    Query()                        = default;
    Query(const Query&)            = delete;
    Query& operator=(const Query&) = delete;
    
    // Query(long _seq, std::string&& _str, std::vector<Expr>&& _exprs)
    //     : seq(_seq), str(_str), exprs(_exprs) {}
    Query(
        long _seq, std::string _str, std::vector<Expr> _exprs, int _maxMatches
    )
        : seq(_seq)
        , str(std::move(_str))
        , exprs(std::move(_exprs))
        , maxMatches(_maxMatches) {}

    // Move constructor (noexcept)
    Query(Query&& other) noexcept
        : seq(other.seq)
        , str(std::move(other.str))
        , exprs(std::move(other.exprs))
        , maxMatches(other.maxMatches) {}

    // Move assignment operator (noexcept)
    Query& operator=(Query&& other) noexcept {
        if (this != &other) {
            seq        = other.seq;
            str        = std::move(other.str);
            exprs      = std::move(other.exprs);
            maxMatches = other.maxMatches;
        }
        return *this;
    }

    [[nodiscard]] Query clone() const {
        return Query{seq, str, exprs, maxMatches};
    }
};

struct StopSignal {};

using Msg = std::variant<Index, Query, StopSignal>;

struct QueryResult {
    Query query;
    // std::vector<json>        jsonLines;
    std::vector<std::string> lines;

    QueryResult() = default;
    QueryResult(
        Query&& q, /*std::vector<json>&& jl,*/ std::vector<std::string>&& l
    )
        : query(std::move(q))
        , /*jsonLines(std::move(jl)),*/ lines(std::move(l)) {}

    // Move constructor (noexcept)
    QueryResult(QueryResult&& other) noexcept
        : query(std::move(other.query))
        // , jsonLines(std::move(other.jsonLines))
        , lines(std::move(other.lines)) {}

    // Move assignment operator (noexcept)
    QueryResult& operator=(QueryResult&& other) noexcept {
        if (this != &other) {
            query = std::move(other.query);
            // jsonLines = std::move(other.jsonLines);
            lines = std::move(other.lines);
        }
        return *this;
    }

    // Delete copy constructor and copy assignment operator
    QueryResult(const QueryResult&)            = delete;
    QueryResult& operator=(const QueryResult&) = delete;
};
