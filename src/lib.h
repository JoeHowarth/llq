#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <cctype>
#include <iterator>

#include "expr.h"
#include "logging.h"
#include "read_file_backwards.h"

namespace ranges = std::ranges;

bool matches(const std::string& key, const std::vector<std::string>& terms) {
    return terms.end() != ranges::find(terms, key);
}

json filterLine(
    json                            line,
    const std::vector<std::string>& terms,
    std::vector<std::string>&       toRemove
) {
    for (const auto& item : line.items()) {
        const std::string& key = item.key();
        if (!matches(key, terms)) {
            toRemove.push_back(key);
        }
    }
    for (const auto& key : toRemove) {
        line.erase(key);
    }
    toRemove.clear();

    return std::move(line);
}

bool opMatches(const Value& val, const Value& rhs, const Expr::Op& op) {
    switch (op) {
        case Expr::Op::eq:
            // Log::info("[FilterLine]: op is eq");
            return val == rhs;
        case Expr::Op::lt:
            // Log::info("[FilterLine]: op is lt");
            return val < rhs;
        case Expr::Op::gt:
            // Log::info("[FilterLine]: op is gt");
            return val > rhs;
        case Expr::Op::in:
            // Log::info("[FilterLine]: op is in");
            break;
        case Expr::Op::fzf:
            // Log::info("[FilterLine]: op is fzf");
            break;
    }
    return true;
}

json filterLine(json line, const std::vector<Expr>& terms) {
    json out;
    for (const Expr& expr : terms) {
        if (!line.contains(expr.path.ptr)) {
            // Log::info(
            //     "[FilterLine]", {{"expr", expr.toJson()}, {"line", line}}
            // );
            // Log::info("[FilterLine] line does not contain expr, early
            // return");
            return {};
        }

        json& json_at_path = line.at(expr.path.ptr);
        // Log::info(
        //     "[FilterLine]", {{"expr", expr.toJson()}, {"atPath",
        //     json_at_path}}
        // );

        if (expr.op) {
            assert(expr.rhs);
            std::optional<Value> val = Value::from_json(json_at_path);
            if (!val) {
                // Log::info(
                //     "Json at path is not a leaf Value",
                //     {{"json_at_path", json_at_path}}
                // );
                return {};
            }
            if (!opMatches(*val, *expr.rhs, *expr.op)) {
                return {};
            }
        }
        out[expr.path.ptr] = line[expr.path.ptr];
    }
    return out;
}
std::string formatResult(const json& line) {
    std::string text;
    text.reserve(256);
    auto inserter = std::back_inserter(text);
    auto begin    = line.items().begin();
    auto end      = line.items().end();
    for (auto it = begin; it != end;) {
        const auto& item = *it;
        fmt::format_to(inserter, "{}: {}", item.key(), item.value().dump());
        ++it;
        if (it == end) {
            break;
        }
        fmt::format_to(inserter, ",  ");
    }
    return std::move(text);
}

std::vector<std::string> formatResults(const std::vector<json>& jsonLines) {
    std::vector<std::string> output;
    output.reserve(jsonLines.size());
    for (const auto& line : jsonLines) {
        output.push_back(std::move(formatResult(line)));
    }
    return output;
}

std::vector<json> runQuery(
    const std::vector<Expr>& exprs,
    const std::string&       logFilename,
    uint                     maxMatches = UINT_MAX,
    uint                     maxLines   = UINT_MAX
) {
    // Open log file
    ReadFileBackwards revReader(logFilename);

    // Read all lines from log file into vector.
    // Prevents infinite loop when this program produces logs to same file
    // as it's reading from
    // TODO: Allow streaming approach with flag

    std::vector<json> result;
    int               i = 0;
    for (const auto& line : revReader) {
        if (i >= maxLines || result.size() >= maxMatches) {
            break;
        }

        json jsonLine = json::parse(line, nullptr, false);
        if (jsonLine.is_discarded()) {
            Log::info(
                "Found discarded line while reading log file", {{"line", line}}
            );
            continue;
        }
        json filtered = filterLine(jsonLine, exprs);
        if (!filtered.is_null()) {
            result.push_back(std::move(filtered));
        }
        ++i;
    }
    return result;
}
