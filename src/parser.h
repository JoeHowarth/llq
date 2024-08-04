#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/core/parse.hpp>
#include <boost/spirit/home/x3/numeric/real.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "expr.h"

namespace x3    = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

namespace parser {

struct Expression {
    std::vector<std::string>                        path;
    std::optional<Expr::Op>                         op;
    std::optional<x3::variant<double, std::string>> rhs;
    // std::string rhs;
};

}  // namespace parser

BOOST_FUSION_ADAPT_STRUCT(parser::Expression, path, op, rhs);

namespace parser {

// Symbol table for operators
struct op_symbols : x3::symbols<Expr::Op> {
    op_symbols() {
        add("<", Expr::Op::lt);
        add("==", Expr::Op::eq);
        add(">", Expr::Op::gt);
        add("in", Expr::Op::in);
        add("fzf", Expr::Op::fzf);
    }
} op_table;

x3::rule<class qs, std::string> single_quoted_string = "qs";

// Define the parser for a single-quoted string
auto const single_quoted_string_def = x3::lexeme
    ['\'' >> x3::lexeme
                 [*(('\\' >> x3::char_('\'')) |  // handle escaped single quotes
                    ('\\' >> x3::char_('\\')) |  // handle escaped backslashes
                    (~x3::char_('\'')))] >>
     '\''];

BOOST_SPIRIT_DEFINE(single_quoted_string);

x3::rule<class glob, std::string> glob     = "glob";
auto const                        glob_def = x3::lexeme[+x3::char_('*')];
BOOST_SPIRIT_DEFINE(glob);

const auto f = [](auto& ctx) {
    _val(ctx) = std::vector<std::string>{_attr(ctx)};
};
x3::rule<class glob2, std::vector<std::string>> glob2     = "glob2";
auto const                                      glob2_def = glob[f];
BOOST_SPIRIT_DEFINE(glob2);

// Define the parser for the path
x3::rule<class path, std::vector<std::string>> path = "path";
auto const path_def = x3::lexeme[(+x3::alpha % '.')];
BOOST_SPIRIT_DEFINE(path);

x3::rule<class rhs, std::optional<x3::variant<double, std::string>>> rhs =
    "rhs";
// Define the parser for the RHS
// auto const rhs = single_quoted_string | x3::double_;
auto const rhs_def = x3::double_ | single_quoted_string;
BOOST_SPIRIT_DEFINE(rhs);

// Define the parser for the expression
x3::rule<class expr, Expression> expr = "expr";
auto const expr_def                   = (path | glob2) >> -op_table >> -(rhs);
BOOST_SPIRIT_DEFINE(expr);

auto const exprs = expr % ',';

// Define the types to hold the parsed attributes
using expr_type = Expression;

void toExpr(const Expression& parsed, Expr& ex) {
    ex.path = std::move(Path(parsed.path));

    if (parsed.op && parsed.rhs) {
        ex.op = parsed.op;
        if (const auto* d = boost::get<double>(&*parsed.rhs)) {
            ex.rhs = Value(*d);
        } else if (const auto* s = boost::get<std::string>(&*parsed.rhs)) {
            ex.rhs = Value(*s);
        }
    }
}

template <typename out_t, typename attr_t, typename Func>
std::optional<out_t>
parseGeneric(const std::string& input, const auto rule, Func func) {
    attr_t parsed;
    out_t  out;
    auto   iter = input.begin();
    auto   end  = input.end();

    bool r = x3::phrase_parse(iter, end, rule, x3::space, parsed);
    if (r && iter == end) {
        func(parsed, out);
        return out;
    }
    return std::nullopt;
}

std::optional<std::vector<Expr>> parseExprs(const std::string& input) {
    using out_t  = std::vector<Expr>;
    using attr_t = std::vector<expr_type>;

    const auto f = [](const attr_t& p_vec, out_t& out) {
        out       = out_t(p_vec.size());
        auto size = p_vec.size();
        for (int i = 0; i < size; ++i) {
            toExpr(p_vec[i], out[i]);
        }
    };
    return parseGeneric<out_t, attr_t>(input, exprs, f);
}
std::optional<Expr> parseExpr(const std::string& input) {
    using out_t  = Expr;
    using attr_t = expr_type;

    return parseGeneric<out_t, attr_t>(input, expr, toExpr);
}

/*
std::optional<Expr> parseExprOld(const std::string& input) {
    expr_type parsed;
    auto      iter = input.begin();
    auto      end  = input.end();

    bool r = x3::phrase_parse(iter, end, expr, x3::space, parsed);
    if (r && iter == end) {
        // TODO: make not trash
        Expr ex;
        toExpr(parsed, ex);
        return std::move(ex);
    }
    return std::nullopt;
}
*/

}  // namespace parser
