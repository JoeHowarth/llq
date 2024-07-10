#pragma once

#include <fmt/core.h>
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
    std::vector<std::string> path;
    Expr::Op op;
    x3::variant<double, std::string> rhs;
    // std::string rhs;
};

} // namespace parser

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
    ['\'' >> x3::lexeme[*(('\\' >> x3::char_('\'')) |  // handle escaped single quotes
                            ('\\' >> x3::char_('\\')) |  // handle escaped backslashes
                            (~x3::char_('\'')))] >>
     '\''];

BOOST_SPIRIT_DEFINE(single_quoted_string);

// Define the parser for the path
auto const path = x3::lexeme[+x3::alpha % '.'];

// Define the parser for the RHS
// auto const rhs = single_quoted_string | x3::double_;
auto const rhs = x3::double_ | single_quoted_string ;

// Define the parser for the expression
auto const expr = path >> op_table >> rhs;

// Define the types to hold the parsed attributes
using expr_type = Expression;

std::optional<expr_type> parseExpr(const std::string& input) {
    expr_type parsed_expr;
    auto iter = input.begin();
    auto end = input.end();
    
    bool r = x3::phrase_parse(iter, end, rhs, x3::space, parsed_expr.rhs);

    // bool r = x3::phrase_parse(iter, end, expr, x3::space, parsed_expr);
    if (r && iter == end) {
        return parsed_expr;
    }
    return std::nullopt;
}

}  // namespace parser
  // namespace parser
