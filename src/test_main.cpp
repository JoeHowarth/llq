#include <fmt/core.h>

#include <algorithm>
#include <optional>
#include <ranges>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <fmt/ranges.h>
#include <folly/Synchronized.h>

#include "bitset.h"
#include "ingestor.h"
#include "lib.h"
#include "parser.h"
#include "query_service.h"
#include "read_file_backwards.h"

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

    s        = "*";
    expected = {"*"};
    res      = split(s, '/');
    fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "/";
    expected = {};
    res      = split(s, '/');
    fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "/hi/bye";
    expected = {"hi", "bye"};
    res      = split(s, '/');
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
        CHECK(expr->path.ptr == ptr);
        CHECK(expr->path == Path("msg"));
    }

    SUBCASE("foo.bar") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path.ptr == ptr);
    }

    SUBCASE("foo.bar == 'hi'") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar == 'hi'");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path.ptr == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::eq);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value("hi"));
    }

    SUBCASE("foo.bar == 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar == 1");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path.ptr == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::eq);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value(1));
    }

    SUBCASE("foo.bar < 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar < 1");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path.ptr == ptr);
        CHECK(expr->op);
        CHECK(*expr->op == Expr::Op::lt);
        CHECK(expr->rhs);
        CHECK(*expr->rhs == Value(1));
    }

    SUBCASE("foo.bar > 1") {
        std::optional<Expr> expr = parser::parseExpr("foo.bar > 1");
        json::json_pointer  ptr  = "/foo/bar"_json_pointer;

        CHECK(expr != std::nullopt);
        CHECK(expr->path.ptr == ptr);
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
    fmt::println("Expr: {}", Expr("*").toJson().dump());
    fmt::println("");
    CHECK(expr == Expr("*"));

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

TEST_CASE("Merge Index with other Index") {
    auto make = [](std::vector<json> lines) {
        Index ind;
        for (auto& line : lines) {
            updateIndex(ind, std::move(line));
        }
        return std::move(ind);
    };
    SUBCASE("disjoint contiguous") {
        fmt::println("");
        Index a     = make({
            {{"msg", "from the future"}},
            {{"count", 23}},
        });
        Index b     = make({
            {{"msg", "bye bye birdie"}},
            {{"count", 25}},
        });
        b.start_idx = 2;
        {
            fmt::println("all bitsets: {}", a.bitsets);
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }

        mergeIndex(a, b);

        json lines = {
            {{"msg", "from the future"}},
            {{"count", 23}},
            {{"msg", "bye bye birdie"}},
            {{"count", 25}},
        };
        for (const auto& line : a.lines) {
            fmt::print("{}, ", line.dump());
        }
        fmt::println("");
        CHECK(a.lines == lines);

        {
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            expected.push_back(true);
            expected.push_back(false);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            expected.push_back(false);
            expected.push_back(true);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
    }

    SUBCASE("Invalid: Gap") {
        fmt::println("");
        Index a     = make({
            {{"msg", "from the future"}},
            {{"count", 23}},
        });
        Index b     = make({
            {{"msg", "bye bye birdie"}},
            {{"count", 25}},
        });
        b.start_idx = 3;
        {
            fmt::println("all bitsets: {}", a.bitsets);
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            CHECK(keyBitSet == expected);
        }

        CHECK(mergeIndex(a, b, false) == false);
        try {
            mergeIndex(a, b, true);
            CHECK(false);
        } catch (const std::runtime_error& e) {
            fmt::println("Expected Error: {}", e.what());
            CHECK(true);
        }
    }

    SUBCASE("overlapping") {
        fmt::println("");
        Index a     = make({
            {{"msg", "from the future"}},
            {{"count", 23}},
        });
        Index b     = make({
            {{"msg", "bye bye birdie"}},
            {{"count", 25}},
        });
        b.start_idx = 1;
        {
            fmt::println("all bitsets: {}", a.bitsets);
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }

        mergeIndex(a, b);

        json lines = {
            {{"msg", "from the future"}},
            {{"count", 23}},
            {{"count", 25}},
        };
        for (const auto& line : a.lines) {
            fmt::print("{}, ", line.dump());
        }
        fmt::println("");
        CHECK(a.lines == lines);

        {
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            expected.push_back(false);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            expected.push_back(true);
            fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
    }
}

TEST_CASE("Run Query") {
    fmt::println("\nRun Query Test");
    auto make = [](std::vector<json> lines) {
        Index ind;
        for (auto& line : lines) {
            updateIndex(ind, std::move(line));
        }
        return std::move(ind);
    };

    Index index = make({
        {{"msg", "from the future"}, {"count", 21}},
        {{"count", 23}},
    });

    folly::Synchronized<QueryResult> queryResult;
    std::function<void()>            onUpdate = []() {};

    SUBCASE("msg only") {
        fmt::println("\nRun Query Test > msg only");
        auto maybeQuery = Query::parse("msg");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        runQuery(index, queryResult, onUpdate, std::move(query));

        {
            auto qr = queryResult.rlock();
            fmt::println("QueryResult.lines.size(): {}", qr->lines.size());
            for (const auto& line : qr->lines) {
                fmt::println("{}", line);
            }
            CHECK(qr->lines.size() == 1);
            std::vector<std::string> expected = {"msg: \"from the future\""};
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("count only") {
        fmt::println("\nRun Query Test > count only");
        auto maybeQuery = Query::parse("count");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        runQuery(index, queryResult, onUpdate, std::move(query));

        {
            auto qr = queryResult.rlock();
            fmt::println("QueryResult.lines.size(): {}", qr->lines.size());
            for (const auto& line : qr->lines) {
                fmt::println("{}", line);
            }
            std::vector<std::string> expected = {"count: 23", "count: 21"};
            CHECK(qr->lines.size() == 2);
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("count > 22 predicate") {
        fmt::println("\nRun Query Test > count > 22");
        auto maybeQuery = Query::parse("count > 22");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        runQuery(index, queryResult, onUpdate, std::move(query));

        {
            auto qr = queryResult.rlock();
            fmt::println("QueryResult.lines.size(): {}", qr->lines.size());
            for (const auto& line : qr->lines) {
                fmt::println("{}", line);
            }
            std::vector<std::string> expected = {"count: 23"};
            CHECK(qr->lines.size() == 1);
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("msg and count == 21") {
        fmt::println("\nRun Query Test > msg, count == 21");
        auto maybeQuery = Query::parse("msg, count == 21");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        runQuery(index, queryResult, onUpdate, std::move(query));

        {
            auto qr = queryResult.rlock();
            fmt::println("QueryResult.lines.size(): {}", qr->lines.size());
            for (const auto& line : qr->lines) {
                fmt::println("{}", line);
            }
            CHECK(qr->lines.size() == 1);
            std::vector<std::string> expected = {
                "msg: \"from the future\",  count: 21"
            };
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("wildcard") {
        fmt::println("\nRun Query Test > *");
        auto maybeQuery = Query::parse("*");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        runQuery(index, queryResult, onUpdate, std::move(query));

        {
            auto qr = queryResult.rlock();
            fmt::println("QueryResult.lines.size(): {}", qr->lines.size());
            for (const auto& line : qr->lines) {
                fmt::println("{}", line);
            }
            CHECK(qr->lines.size() == 2);
            std::vector<std::string> expected = {
                "count: 23",
                "msg: \"from the future\",  count: 21"
            };
            CHECK(qr->lines == expected);
        }
    }
}
