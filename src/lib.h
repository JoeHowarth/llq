#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iterator>
#include <thread>

#include "expr.h"
#include "logging.h"
#include "read_file_backwards.h"

namespace ranges = std::ranges;

void sleep(int millis) {
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

void trimInPlace(std::string& str) {
    // strangely std::isspace doesn't work directly
    auto isspace = [](auto c) { return !std::isspace(c); };
    auto b       = [&str]() { return str.begin(); };
    auto e       = [&str]() { return str.end(); };

    // erase from start to first space
    str.erase(str.begin(), std::find_if(b(), e(), isspace));

    // erase from end to last space
    str.erase(
        std::find_if(str.rbegin(), str.rend(), isspace).base(), str.end()
    );
}

std::string trim(std::string str) {
    trimInPlace(str);
    return std::move(str);
}

std::vector<std::string>
split(const std::string& str, char del, uint take = -1) {
    if (str.size() == 0) {
        return {};
    }
    std::vector<std::string> ret;
    size_t                   start = 0;

    for (int i = 0; i < take && start < str.length(); ++i) {
        size_t ind = str.find(del, start);
        if (ind == std::string::npos) {
            break;
        }
        ret.emplace_back(str.substr(start, ind - start));
        start = ind + 1;
    }
    ret.emplace_back(str.substr(start));
    return ret;
}

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
        if (!line.contains(expr.path)) {
            // Log::info(
            //     "[FilterLine]", {{"expr", expr.toJson()}, {"line", line}}
            // );
            // Log::info("[FilterLine] line does not contain expr, early
            // return");
            return {};
        }

        json& json_at_path = line.at(expr.path);
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
        out[expr.path] = line[expr.path];
    }
    return out;
}

std::vector<std::string> formatResults(const std::vector<json>& jsonLines) {
    std::vector<std::string> output;
    for (const auto& line : jsonLines) {
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
        output.push_back(std::move(text));
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
