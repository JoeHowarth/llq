#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

#include "logging.h"
#include "expr.h"

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

// struct Expr {
//     enum class Op {
//         lt,
//         eq,
//         gt,
//         in,
//         fzf
//     };
//
//     json::json_pointer  path;
//     std::optional<Op>   op;
//     std::optional<json> rhs;
//
//     explicit Expr(const std::string& str) {
//         auto majorTerms = split(str, ' ', 2);
//         std::erase_if(majorTerms, [](const std::string& s) {
//             return s == " ";
//         });
//
//         Log::info("Major Terms", {{"majorTerms", majorTerms}});
//
//         // parse path
//         if (majorTerms[0] == "*") {
//             path = json::json_pointer("/");
//         } else {
//             ranges::replace(majorTerms[0], '.', '/');
//             path = json::json_pointer("/" + majorTerms[0]);
//         }
//
//         if (majorTerms.size() == 1) {
//             return;
//         }
//
//         // parse op
//         const std::string& opStr = majorTerms[1];
//         if (opStr == "<") {
//             op = Op::lt;
//         } else if (opStr == "==") {
//             op = Op::eq;
//         } else if (opStr == ">") {
//             op = Op::gt;
//         } else {
//             Log::info("Unknown op string", {{"opStr", opStr}});
//         }
//
//         // parse rhs
//         rhs = json::parse(majorTerms[2]);
//     }
//
//     [[nodiscard]] json toJson() const {
//         json j = {
//             {"path", this->path.to_string()},
//         };
//
//         if (op) {
//             j["op"] = static_cast<int>(*this->op);
//         };
//         if (rhs) {
//             j["rhs"] = (*this->rhs);
//         }
//         return j;
//     }
// };

// std::vector<std::string> termsFromQuery(const std::string& query) {
//     auto terms = split(query, ',');
//     ranges::for_each(terms, trimInPlace);
//     return terms;
// }
//
// std::vector<Expr> exprsFromQuery(const std::string& query) {
//     auto terms = termsFromQuery(query);
//
//     std::vector<Expr> exprs;
//     exprs.reserve(terms.size());
//     for (const auto& term : terms) {
//         exprs.emplace_back(term);
//     }
//     return exprs;
// }

json filterLine(json line, const std::vector<Expr>& terms) {
    // json flat = line.flatten();
    json out;
    for (const Expr& expr : terms) {
        if (!line.contains(expr.path)) {
            Log::info("[FilterLine]", {{"expr", expr.toJson()}});
            Log::info("[FilterLine] line does not contain expr, early return");
            return {};
        }

        json& val = line.at(expr.path);
        Log::info("[FilterLine]", {{"expr", expr.toJson()}, {"atPath", val}});

        if (expr.op) {
            assert(expr.rhs);
            switch (*expr.op) {
                case Expr::Op::eq:
                    Log::info("[FilterLine]: op is eq");
                    if (Value::from_json(val) != *expr.rhs) {
                        return {};
                    }
                    break;
                case Expr::Op::lt:
                    Log::info("[FilterLine]: op is lt");
                    break;
                case Expr::Op::gt:
                    Log::info("[FilterLine]: op is gt");
                    break;
                case Expr::Op::in:
                    Log::info("[FilterLine]: op is in");
                    break;
                case Expr::Op::fzf:
                    Log::info("[FilterLine]: op is fzf");
                    break;
            }
        }

        out[expr.path] = line[expr.path];
    }
    return out;
}

/*
void runQuery(const std::string& rawQuery) {
    // Parse raw query string into list of expressions
    auto exprs = exprsFromQuery(rawQuery);

    // Open log file
    std::ifstream logFile("log.json");

    // Read all lines from log file into vector.
    // Prevents infinite loop when this program produces logs to same file
    // as it's reading from
    // TODO: Allow streaming approach with flag
    std::vector<std::string> lines;
    std::string              rawLine;
    while (std::getline(logFile, rawLine)) {
        lines.push_back(rawLine);
    }

    // filter and display lines
    for (const auto& line : lines) {
        json filtered = filterLine(json::parse(line), exprs);
        if (!filtered.is_null()) {
            for (const auto& item : filtered.items()) {
                fmt::print("{}: {},  ", item.key(), item.value().dump());
            }
            fmt::println("");
        }
    }
    fmt::println("");
}
*/
