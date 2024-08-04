#pragma once

#include <fmt/core.h>

#include <boost/fusion/include/adapt_struct.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "string_utils.h"

using json = nlohmann::ordered_json;

struct Value {
   public:
    static std::optional<Value> from_json(const json& val) {
        if (val.is_number()) {
            return Value(val.get<double>());
        }
        if (val.is_string()) {
            return Value(val.get<std::string>());
        }
        return std::nullopt;
    }

    explicit Value(double val) : v(val) {}
    explicit Value(const std::string& val) : v(val) {}

    // Addition
    Value operator+(const Value& other) const {
        if (const auto* lhs = std::get_if<double>(&v)) {
            if (const auto* rhs = std::get_if<double>(&other.v)) {
                return Value(*lhs + *rhs);
            }
        }
        throw std::invalid_argument("Invalid types for addition");
    }

    // Subtraction
    Value operator-(const Value& other) const {
        if (const auto* lhs = std::get_if<double>(&v)) {
            if (const auto* rhs = std::get_if<double>(&other.v)) {
                return Value(*lhs - *rhs);
            }
        }
        throw std::invalid_argument("Invalid types for subtraction");
    }

    // Multiplication
    Value operator*(const Value& other) const {
        if (const auto* lhs = std::get_if<double>(&v)) {
            if (const auto* rhs = std::get_if<double>(&other.v)) {
                return Value(*lhs * *rhs);
            }
        }
        throw std::invalid_argument("Invalid types for subtraction");
    }

    // Division
    Value operator/(const Value& other) const {
        if (const auto* lhs = std::get_if<double>(&v)) {
            if (const auto* rhs = std::get_if<double>(&other.v)) {
                return Value(*lhs / *rhs);
            }
        }
        throw std::invalid_argument("Invalid types for subtraction");
    }

    // Comparison
    bool operator==(const Value& other) const {
        return v == other.v;
    }
    bool operator<(const Value& other) const {
        return v < other.v;
    }
    bool operator>(const Value& other) const {
        return v > other.v;
    }

    [[nodiscard]] bool is_num() const {
        return std::holds_alternative<double>(v);
    }
    [[nodiscard]] bool is_str() const {
        return std::holds_alternative<std::string>(v);
    }
    double& get_num() {
        return std::get<double>(v);
    }
    std::string& get_str() {
        return std::get<std::string>(v);
    }

    [[nodiscard]] std::string to_string() const {
        if (const auto* val = std::get_if<double>(&v)) {
            return fmt::format("{}", *val);
        }
        const auto* val = std::get_if<std::string>(&v);
        return fmt::format("{}", *val);
    }

   private:
    std::variant<std::string, double> v;
};

struct Path {
    json::json_pointer ptr{""};
    std::size_t        frontHash{};
    bool               isWildCard = false;

    Path() = default;

    explicit Path(const std::string& str) {
        if (str.size() == 1 && str[0] == '*') {
            isWildCard = true;
            return;
        }

        make(split(str, '/'));
    }

    explicit Path(const std::vector<std::string>& segments) {
        if (segments.size() == 1 && segments[0][0] == '*') {
            isWildCard = true;
            return;
        }
        make(segments);
    }

    bool operator==(const Path& other) const {
        if (this->frontHash != other.frontHash) {
            return false;
        }
        return this->ptr == other.ptr;
    }

    [[nodiscard]] std::string to_string() const {
        return ptr.to_string();
    }

   private:
    void make(const std::vector<std::string>& segments) {
        for (const auto& seg : segments) {
            ptr.push_back(seg);
        }
        frontHash = std::hash<std::string>()(segments.front());
    }
};

class Expr {
   public:
    enum class Op {
        lt,
        eq,
        gt,
        in,
        fzf
    };

    Path path;
    // TODO: combine Op and Value under single optional to prevent malformed
    // instance by construction
    std::optional<Op>    op;
    std::optional<Value> rhs;

    Expr() = default;
    explicit Expr(Path&& _path) : path(std::move(_path)) {}
    explicit Expr(Path _path) : path(std::move(_path)) {}
    explicit Expr(std::vector<std::string>& _path) : path(_path) {}
    explicit Expr(const std::string& _path) : path(_path) {}
    Expr(Path _path, Op _op, Value _rhs)
        : path(std::move(_path)), op(_op), rhs(std::move(_rhs)) {}

    [[nodiscard]] std::string to_string() const {
        if (op) {
            return fmt::format(
                "{} {} {}", path.to_string(), op_str(*op), rhs->to_string()
            );
        }
        return path.to_string();
    }

    [[nodiscard]] bool matches(const json& line) const {
        if (!line.contains(path.ptr)) {
            return false;
        }
        return opMatches(line);
    }

    [[nodiscard]] bool opMatches(const json& line) const {
        if (!op) {
            // trivially true
            return true;
        }
        // rhs better be there if op is
        assert(rhs);
        auto val = Value::from_json(line.at(path.ptr));
        if (!val) {
            return false;
        }

        switch (*op) {
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
                return false;
            case Expr::Op::fzf:
                // Log::info("[FilterLine]: op is fzf");
                return false;
        }
    }

    static std::string op_str(Op op) {
        std::hash<std::string>()("hi");

        switch (op) {
            case Op::lt:
                return "<";
            case Op::eq:
                return "==";
            case Op::gt:
                return ">";
            case Op::in:
                return "in";
            case Op::fzf:
                return "fzf";
        };
    }

    [[nodiscard]] json toJson() const {
        json j = {
            {"path", this->path.to_string()},
        };

        if (op) {
            j["op"] = op_str(*op);
        };
        if (rhs) {
            j["rhs"] = (rhs->to_string());
        }
        return j;
    }

    bool operator==(const Expr& a) const {
        return path == a.path && op == a.op && rhs == a.rhs;
    }
};

// fmt::formatter specialization for Value
template <>
struct fmt::formatter<Value> {
    static constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Value& val, FormatContext& ctx) {
        return format_to(ctx.out(), "{}", val.to_string());
    }
};

// fmt::formatter specialization for Expr
template <>
struct fmt::formatter<Expr> {
    static constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Expr& e, FormatContext& ctx) {
        format_to(ctx.out(), "{}", e.to_string());
    }
};

BOOST_FUSION_ADAPT_STRUCT(Expr, path)
