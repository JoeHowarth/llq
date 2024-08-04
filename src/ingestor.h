#pragma once

#include <folly/MPMCQueue.h>

#include <stdexcept>
#include <unordered_map>

#include "bitset.h"
#include "lib.h"
#include "types.h"

void updateIndex(Index& index, json&& obj) {
    auto        keyHash = std::hash<std::string>();
    std::size_t lineNum = index.lines.size();
    for (const auto& it : obj.items()) {
        const auto& key  = it.key();
        std::size_t hash = keyHash(key);
        index.bitsets[hash].set(lineNum, true);
        fmt::println("key {}, hash {}, bs {}", key, hash, index.bitsets[hash]);
    }
    index.lines.push_back(std::move(obj));
}

// call like: std::thread producerThread(startIngesting, std::ref(queue),
// std::ref(iFileStream));
void startIngesting(folly::MPMCQueue<Msg>& sender, std::istream& file) {
    Index          index;
    std::string    line;
    std::streampos lastPosition;
    std::size_t    lastLineNumberSent;

    auto resetAndSend = [&]() {
        // send index if it's non-empty
        if (index.lines.size() > 0) {
            if (index.start_idx > lastLineNumberSent + 1) {
                throw std::runtime_error(
                    "Tried to send an Index with start_idx greater than 1 + "
                    "lastLineNumberSent"
                );
            }
            auto new_start_idx = index.start_idx + index.lines.size();
            lastLineNumberSent = new_start_idx - 1;
            sender.blockingWrite(std::move(index));
            // reset index after sending
            index.start_idx = new_start_idx;
            index.lines.clear();
            index.bitsets.clear();
        }

        file.clear();              // Clear the EOF flag
        file.seekg(lastPosition);  // Reset cursor to the last position
        // Wait before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    while (true) {
        lastPosition = file.tellg();
        std::getline(file, line);
        if (file.eof()) {
            resetAndSend();
            continue;  // Check again for new data
        }

        // Parse the JSON line
        nlohmann::json jsonObject = nlohmann::json::parse(line);
        if (jsonObject.is_discarded()) {
            resetAndSend();
            continue;
        }

        updateIndex(index, jsonObject);
    }
}
