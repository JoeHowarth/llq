#pragma once

#define DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
#include <fmt/core.h>
#include <fmt/format.h>

#include <boost/dynamic_bitset.hpp>
#include <boost/dynamic_bitset/dynamic_bitset.hpp>

#include "doctest.h"

class BitSet {
   private:
    boost::dynamic_bitset<> bitset_;
    std::size_t             m_size{};

   public:
    static BitSet trueMask(std::size_t size) {
        BitSet bs(size);
        bs.m_size = size;
        bs.bitset_.flip();
        return std::move(bs);
    }

    static BitSet falseMask(std::size_t size) {
        BitSet bs(size);
        bs.m_size = size;
        return std::move(bs);
    }

    explicit BitSet(std::size_t capacity = 1024) : bitset_(capacity) {}
    explicit BitSet(boost::dynamic_bitset<> _bs) : bitset_(std::move(_bs)) {
        m_size = _bs.size();
    }

    void push_back(bool value) {
        if (m_size == bitset_.size()) {
            bitset_.resize(bitset_.size() * 2);
        }
        bitset_.set(m_size, value);
        ++m_size;
    }

    [[nodiscard]] std::size_t capacity() const {
        return bitset_.size();
    }

    [[nodiscard]] std::size_t size() const {
        return m_size;
    }

    void set(std::size_t idx, bool value) {
        while (size() <= idx) {
            push_back(false);
        }
        bitset_.set(idx, value);
    }

    bool operator[](std::size_t index) const {
        return bitset_[index];
    }

    bool operator==(const BitSet& other) const {
        return this->bitset_ == other.bitset_;
    }

    BitSet& operator&=(const BitSet& other) {
        this->m_size = std::min(this->m_size, other.m_size);
        this->bitset_.resize(other.capacity());
        this->bitset_ &= other.bitset_;
        return *this;
    }

    BitSet operator&(const BitSet& other) const {
        BitSet result = *this;
        result &= other;
        return result;
    }

    BitSet operator|(const BitSet& other) const {
        BitSet result;
        result.bitset_ = this->bitset_ | other.bitset_;
        result.m_size  = std::max(this->m_size, other.m_size);
        return result;
    }

    BitSet operator^(const BitSet& other) const {
        BitSet result;
        result.bitset_ = this->bitset_ ^ other.bitset_;
        result.m_size  = std::max(this->m_size, other.m_size);
        return result;
    }

    BitSet operator~() const {
        BitSet result(*this);
        result.bitset_ = ~this->bitset_;
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const BitSet& bitset) {
        for (std::size_t i = 0; i < bitset.m_size; ++i) {
            os << bitset.bitset_[i];
        }
        return os;
    }

    class TrueIndexIterator {
       private:
        const BitSet& bitset_;
        std::size_t   current_;
        bool          reverse_;

       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::size_t;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const std::size_t*;
        using reference         = const std::size_t&;

        TrueIndexIterator(
            const BitSet& bitset, std::size_t start, bool reverse = false
        )
            : bitset_(bitset), current_(start), reverse_(reverse) {
            if (!reverse_) {
                // Advance to the first true bit
                while (current_ < bitset_.size() && !bitset_.bitset_[current_]
                ) {
                    ++current_;
                }
            } else {
                // Retreat to the last true bit
                while (current_ != static_cast<std::size_t>(-1) &&
                       !bitset_.bitset_[current_]) {
                    --current_;
                }
            }
        }

        std::size_t operator*() const {
            return current_;
        }

        TrueIndexIterator& operator++() {
            if (!reverse_) {
                do {
                    ++current_;
                } while (current_ < bitset_.size() && !bitset_.bitset_[current_]
                );
            } else {
                do {
                    if (current_ == 0) {
                        current_ = static_cast<std::size_t>(-1);
                        break;
                    }
                    --current_;
                } while (current_ != static_cast<std::size_t>(-1) &&
                         !bitset_.bitset_[current_]);
            }
            return *this;
        }

        bool operator!=(const TrueIndexIterator& other) const {
            return current_ != other.current_;
        }
    };

    [[nodiscard]] TrueIndexIterator begin() const {
        return TrueIndexIterator(*this, 0);
    }

    [[nodiscard]] TrueIndexIterator end() const {
        return TrueIndexIterator(*this, m_size);
    }

    [[nodiscard]] TrueIndexIterator rbegin() const {
        return TrueIndexIterator(*this, m_size - 1, true);
    }

    [[nodiscard]] TrueIndexIterator rend() const {
        return TrueIndexIterator(*this, static_cast<std::size_t>(-1), true);
    }
};

template <>
struct fmt::formatter<BitSet> {
    char separator = '\0';

    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            separator = *it++;
            if (separator != ',' && separator != ' ') {
                throw fmt::format_error("invalid format");
            }
        }
        return it;
    }

    template <typename FormatContext>
    auto
    format(const BitSet& bitset, FormatContext& ctx) const -> decltype(ctx.out()
                                                           ) {
        auto out = ctx.out();

        *out++ = '{';
        for (std::size_t i = 0; i < bitset.size(); ++i) {
            if (i > 0 && separator != '\0') {
                *out++ = separator;
            }
            fmt::format_to(out, "{}", static_cast<int>(bitset[i]));
        }
        *out++ = '}';
        return out;
    }
};

TEST_CASE("BitSet operations") {
    BitSet bitset1;
    BitSet bitset2;

    // Add bits to the bitsets
    bitset1.push_back(true);
    bitset1.push_back(false);
    bitset1.push_back(true);
    bitset1.push_back(true);

    bitset2.push_back(false);
    bitset2.push_back(true);
    bitset2.push_back(true);
    bitset2.push_back(false);

    CHECK(bitset1.size() == 4);
    CHECK(bitset2.size() == 4);

    // Perform bitwise operations
    BitSet andResult = bitset1 & bitset2;
    BitSet orResult  = bitset1 | bitset2;
    BitSet xorResult = bitset1 ^ bitset2;
    BitSet notResult = ~bitset1;

    // Check the results
    CHECK(andResult.size() == 4);
    CHECK(orResult.size() == 4);
    CHECK(xorResult.size() == 4);
    CHECK(notResult.size() == 4);

    // Check individual bits
    CHECK(andResult[0] == false);
    CHECK(andResult[1] == false);
    CHECK(andResult[2] == true);
    CHECK(andResult[3] == false);

    CHECK(orResult[0] == true);
    CHECK(orResult[1] == true);
    CHECK(orResult[2] == true);
    CHECK(orResult[3] == true);

    CHECK(xorResult[0] == true);
    CHECK(xorResult[1] == true);
    CHECK(xorResult[2] == false);
    CHECK(xorResult[3] == true);

    CHECK(notResult[0] == false);
    CHECK(notResult[1] == true);
    CHECK(notResult[2] == false);
    CHECK(notResult[3] == false);
}

TEST_CASE("BitSet push_back and capacity") {
    BitSet bitset;

    CHECK(bitset.size() == 0);
    CHECK(bitset.capacity() >= 1024);

    bitset.push_back(true);
    bitset.push_back(false);
    bitset.push_back(true);
    bitset.push_back(true);

    CHECK(bitset.size() == 4);
    CHECK(bitset[0] == true);
    CHECK(bitset[1] == false);
    CHECK(bitset[2] == true);
    CHECK(bitset[3] == true);

    for (std::size_t i = 0; i < 1024; ++i) {
        bitset.push_back(i % 2 == 0);
    }

    CHECK(bitset.size() == 1028);
    CHECK(bitset.capacity() >= 2048);  // Capacity should have doubled
}

TEST_CASE("BitSet TrueIndexIterator forward") {
    BitSet bitset;

    bitset.push_back(false);
    bitset.push_back(true);
    bitset.push_back(false);
    bitset.push_back(true);
    bitset.push_back(true);

    std::vector<std::size_t> trueIndexes;
    for (unsigned long it : bitset) {
        trueIndexes.push_back(it);
    }

    CHECK(trueIndexes.size() == 3);
    CHECK(trueIndexes[0] == 1);
    CHECK(trueIndexes[1] == 3);
    CHECK(trueIndexes[2] == 4);
}

TEST_CASE("BitSet TrueIndexIterator reverse") {
    BitSet bitset;

    bitset.push_back(false);
    bitset.push_back(true);
    bitset.push_back(false);
    bitset.push_back(true);
    bitset.push_back(true);

    std::vector<std::size_t> trueIndexes;
    for (auto it = bitset.rbegin(); it != bitset.rend(); ++it) {
        trueIndexes.push_back(*it);
    }

    CHECK(trueIndexes.size() == 3);
    CHECK(trueIndexes[0] == 4);
    CHECK(trueIndexes[1] == 3);
    CHECK(trueIndexes[2] == 1);
}

TEST_CASE("Masks") {
    auto ones = BitSet::trueMask(3);
    CHECK(ones.size() == 3);
    CHECK(ones.capacity() == 3);
    CHECK(ones[0] == true);
    CHECK(ones[1] == true);
    CHECK(ones[2] == true);

    auto zeroes = BitSet::falseMask(3);
    CHECK(zeroes.size() == 3);
    CHECK(zeroes.capacity() == 3);
    CHECK(zeroes[0] == false);
    CHECK(zeroes[1] == false);
    CHECK(zeroes[2] == false);
}

TEST_CASE("BitSet set") {
    BitSet bitset;

    bitset.push_back(false);
    bitset.push_back(true);

    bitset.set(2, true);
    CHECK(bitset[2] == true);

    bitset.set(5, true);
    CHECK(bitset.size() == 6);
    CHECK(bitset.capacity() >= 6);
    CHECK(bitset[2] == true);
    CHECK(bitset[3] == false);
    CHECK(bitset[4] == false);
    CHECK(bitset[5] == true);

    bitset.set(2, false);
    CHECK(bitset[2] == false);
}
