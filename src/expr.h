#pragma once

#include <fmt/core.h>

#include <boost/fusion/include/adapt_struct.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

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

class Expr {
   public:
    enum class Op {
        lt,
        eq,
        gt,
        in,
        fzf
    };

    static std::optional<Expr> make(const std::vector<std::string>& path, std::optional<Op> op, std::optional<std::variant<std::string, double>> rhs) {
        return std::nullopt;
    }

    json::json_pointer   path;
    std::optional<Op>    op;
    std::optional<Value> rhs;

    [[nodiscard]] std::string to_string() const {
        std::string s = path.to_string();
        if (op) {
            return fmt::format("{} {} {}", 
path.to_string(), 
                               op_str(*op),
                               rhs->to_string()
                               );
        }
        return s;
    }

    static std::string op_str(Op op) {
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
