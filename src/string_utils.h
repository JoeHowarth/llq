#pragma once

#include <fmt/core.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

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
        if (start < ind ) {
            ret.emplace_back(str.substr(start, ind - start));
        }
        start = ind + 1;
    }
    if (start < str.size()) {
        ret.emplace_back(str.substr(start));
    }
    return ret;
}
