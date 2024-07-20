#include <thread>

#include "logging.h"

int main() {
    Log::init("dummy_log.json");
    Log::info("First log");

    int count = 0;
    while (true) {
        using namespace std::chrono_literals;

        Log::info("In the loop babyyy", {{"count", count}});
        if ((count % 2) == 0) {
            Log::info("Even", {{"count", count}, {"tag", "even"}});
        }
        if ((count % 3) == 0) {
            Log::info(
                "Multiple of three", {{"count", count}, {"tag", "three"}}
            );
        }
        if ((count % 5) == 0) {
            Log::info("Multiple of 5", {{"count", count}, {"tag", "five"}});
        }
        if ((count % 7) == 0) {
            Log::info("Multiple of 7", {{"count", count}, {"tag", "7"}});
        }
        Log::info(
            "Using some strings too", {{"name", fmt::format("Bobby {}", count)}}
        );

        count++;

        std::this_thread::sleep_for(200ms);
    }
}
