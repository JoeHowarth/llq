#pragma once

#include <fmt/core.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
// using json = nlohmann::json;
using json = nlohmann::ordered_json;

namespace Log {

enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

std::ofstream fp;
bool          sendToCout = true;

void init(const std::string& filepath) {
    fp = std::ofstream(filepath);
}

void toStr(Level level, std::string& str) {
    switch (level) {
        case Level::Trace:
            str.append("trace");
            break;
        case Level::Debug:
            str.append("debug");
            break;
        case Level::Info:
            str.append("info");
            break;
        case Level::Warn:
            str.append("warn");
            break;
        case Level::Error:
            str.append("error");
            break;
        case Level::Critical:
            break;
            str.append("critical");
            break;
    };
}

std::string toStr(Level level) {
    std::string s;
    s.reserve(8);
    toStr(level, s);
    return s;
}

// Logging(const std::string& filepath) : fp(filepath) {}

void log(Level level, const std::string& msg, const json& arg) {
    json line = {{"level", toStr(level)}, {"msg", msg}};

    for (const auto& item : arg.items()) {
        line[item.key()] = item.value();
    }
    
    std::string s = line.dump();
    fp << s << std::endl;
    fp.flush();
    if (sendToCout) {
        std::cout << s << std::endl;
    }
}

void info(const std::string& msg, const json& arg) {
    log(Level::Info, msg, arg);
}

void info(const std::string& msg, const json&& arg) {
    log(Level::Info, msg, arg);
}

void info(const std::string& msg) {
    log(Level::Info, msg, {});
}

json merge(json&& a, const json& b) {
    a.update(b);
   return std::move(a); 
}

}  // namespace Log
