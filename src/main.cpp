#include <fmt/core.h>
#include "lib.h"
#include "logging.h"
#include "parser.h"


int main() {
    Log::init("log.json");
    Log::sendToCout = false;

    Log::info("Hello from Live Log Query (llq)!");

    Log::info("hi", {{"tick", 2}});
    Log::info("bye", {{"thing", {1, 2, 3}}});

    std::string rawQuery;
    while (true) {
        std::getline(std::cin, rawQuery);
        // runQuery(rawQuery);
        parser::parseExpr(rawQuery);
        rawQuery.clear();
    }
}
