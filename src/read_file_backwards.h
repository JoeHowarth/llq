#pragma once

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

class ReadFileBackwards {
   public:
    class Iterator {
       public:
        using iterator_category = std::input_iterator_tag;
        using value_type        = std::string;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const std::string*;
        using reference         = const std::string&;

        Iterator(std::ifstream* file, std::streampos pos)
            : file(file), pos(pos) {
            if ((file != nullptr) && pos >= 0) {
                readNextLine();
            }
        }

        reference operator*() const {
            return currentLine;
        }
        pointer operator->() const {
            return &currentLine;
        }

        Iterator& operator++() {
            readNextLine();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return file == other.file && pos == other.pos;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

       private:
        void readNextLine() {
            currentLine.clear();
            if (pos < 0) {
                file = nullptr;
                return;
            }

            file->seekg(pos);
            char ch;
            while (pos >= 0) {
                file->seekg(pos--);
                file->get(ch);
                if (ch == '\n') {
                    break;
                }
                currentLine += ch;
            }
            std::reverse(currentLine.begin(), currentLine.end());

            if (currentLine.empty() && pos < 0) {
                file = nullptr;
            }
        }

        std::ifstream* file;
        std::streamoff pos;
        std::string    currentLine;
    };

    explicit ReadFileBackwards(const std::string& filePath)
        : file(filePath, std::ios::binary) {
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open the file.");
        }
        file.seekg(0, std::ios::end);
        endPos = file.tellg();
    }

    Iterator begin() {
        return {&file, endPos - 1};
    }

    static Iterator end() {
        return {nullptr, -1};
    }

   private:
    std::ifstream  file;
    std::streamoff endPos;
};
