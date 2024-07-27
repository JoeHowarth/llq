#include <fmt/core.h>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <fmt/ranges.h>

#include "doctest.h"
#include "lib.h"
#include "parser.h"
#include "read_file_backwards.h"

/*
TEST_CASE("split") {
    std::string              s        = "msg,level";
    std::vector<std::string> expected = {"msg", "level"};
    auto                     res      = split(s, ',');
    fmt::println("Res: {}", res);
    CHECK(res == expected);

    s        = "msg";
    expected = {"msg"};
    res      = split(s, ',');
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "";
    expected = {};
    res      = split(s, ',');
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "msg, level\n";
    expected = {"msg", " level\n"};
    res      = split(s, ',');
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "foo.bar == 1";
    expected = {"foo.bar", "==", "1"};
    res      = split(s, ' ');
    fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
    CHECK(res.size() == expected.size());
    CHECK(res == expected);
}

TEST_CASE("trim") {
    std::string s;
    std::string e;

    CHECK(trim("  x") == "x");

    CHECK(trim("x  ") == "x");

    CHECK(trim(" x y") == "x y");

    CHECK(trim("  this is cool  ; ") == "this is cool  ;");

    s = "  x";
    trimInPlace(s);
    CHECK(s == "x");

    s = "x  ";
    trimInPlace(s);
    CHECK(s == "x");

    s = " x  ";
    trimInPlace(s);
    CHECK(s == "x");

    s = "  this is cool  ; ";
    trimInPlace(s);
    CHECK(s == "this is cool  ;");
}
*/

/*
TEST_CASE("Parse terms from query") {
    CHECK(termsFromQuery("") == std::vector<std::string>{});
    CHECK(termsFromQuery("msg") == std::vector<std::string>{"msg"});
    CHECK(
        termsFromQuery("msg,level") == std::vector<std::string>{"msg", "level"}
    );
    CHECK(
        termsFromQuery("msg, level\n") ==
        std::vector<std::string>{"msg", "level"}
    );
    CHECK(
        termsFromQuery(" msg ,  level ") ==
        std::vector<std::string>{"msg", "level"}
    );
}
*/

TEST_CASE("Parse expression") {
    std::string s;

    SUBCASE("msg") {
        std::optional<Expr> expr = parser::parseExpr("msg");
        json::json_pointer  ptr  = "/msg"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path == ptr);
    }

    SUBCASE("foo.bar") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path == ptr);
    }

    SUBCASE("foo.bar == 'hi'") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar == 'hi'");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::eq);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value("hi"));
    }

    SUBCASE("foo.bar == 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar == 1");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::eq);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value(1));
    }

    SUBCASE("foo.bar < 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar < 1");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::lt);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value(1));
    }

    SUBCASE("foo.bar > 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar > 1");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::gt);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value(1));
    }

    SUBCASE("foo.bar > >") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar > >");

        CHECK(expr == std::nullopt);
    }

    SUBCASE("foo.bar 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar > >");

        CHECK(expr == std::nullopt);
    }
}

TEST_CASE("Glob Expr *") {
    fmt::println("");
    fmt::println("Glob expr");
    auto expr = parser::parseExpr("*");
    REQUIRE(expr != std::nullopt);
    fmt::println("Expr: {}", expr->toJson().dump());
    fmt::println("");
    CHECK(expr == Expr(""));

    json line = {{"msg", "hi"}, {"level", 2}, {"foo", "bye"}};
    auto out  = filterLine(line, {*expr});
    CHECK(out == line);
}

TEST_CASE("Parse line into exprs") {
    {
        std::string                      q     = "msg, x == 2";
        std::optional<std::vector<Expr>> exprs = parser::parseExprs(q);

        REQUIRE(exprs != std::nullopt);
        CHECK(exprs->size() == 2);
        fmt::println("Expr 0: {}", (*exprs)[0].toJson().dump());
        fmt::println("Expr 1: {}", (*exprs)[1].toJson().dump());
        CHECK((*exprs)[0].path.to_string() == "/msg");
        CHECK((*exprs)[1].path.to_string() == "/x");
        CHECK(*(*exprs)[1].op == Expr::Op::eq);
        CHECK(*(*exprs)[1].rhs == Value(2));
    }
}

TEST_CASE("Filter Line by Exprs") {
    SUBCASE("msg; passes") {
        auto exprs = *parser::parseExprs("msg");
        json line  = {{"msg", "hi msg"}, {"x", 2}};
        json out   = filterLine(line, exprs);

        fmt::println("out: {}", out.dump());
        CHECK(json({{"msg", "hi msg"}}) == out);
        fmt::println("");
    }

    SUBCASE("msg, x == 2; passes") {
        auto exprs = *parser::parseExprs("msg, x == 2");
        json line  = {{"msg", "hi msg"}, {"x", 2}};
        json out   = filterLine(line, exprs);

        fmt::println("out: {}", out.dump());
        CHECK(line == out);
        fmt::println("");
    }

    SUBCASE("msg, x == 1; fails") {
        auto exprs = *parser::parseExprs("msg, x == 1");
        json line  = {{"msg", "hi msg"}, {"x", 2}};
        json out   = filterLine(line, exprs);

        fmt::println("out: {}", out.dump());
        CHECK(json() == out);
        fmt::println("");
    }
}
