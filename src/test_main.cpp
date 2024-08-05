#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <folly/MPMCQueue.h>
#include <folly/Synchronized.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <thread>

#include "bitset.h"
#include "ingestor.h"
#include "logging.h"
#include "parser.h"
#include "query_service.h"
#include "types.h"

TEST_CASE("split") {
    std::string              s        = "msg,level";
    std::vector<std::string> expected = {"msg", "level"};
    auto                     res      = split(s, ',');
    // fmt::println("Res: {}", res);
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
    // fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "*";
    expected = {"*"};
    res      = split(s, '/');
    // fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "/";
    expected = {};
    res      = split(s, '/');
    // fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
    CHECK(res.size() == expected.size());
    CHECK(res == expected);

    s        = "/hi/bye";
    expected = {"hi", "bye"};
    res      = split(s, '/');
    // fmt::println("s: {}, expected: {}, res: {}", s, expected, res);
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
    // fmt::println("");
    // fmt::println("Glob expr");
    auto expr = parser::parseExpr("*");
    REQUIRE(expr != std::nullopt);
    // fmt::println("Expr: {}", expr->toJson().dump());
    // fmt::println("Expr: {}", Expr("*").toJson().dump());
    // fmt::println("");
    CHECK(*expr == Expr("*"));
    CHECK(expr->path.ptr == json::json_pointer(""));
}

TEST_CASE("Parse line into exprs") {
    {
        std::string                      q     = "msg, x == 2";
        std::optional<std::vector<Expr>> exprs = parser::parseExprs(q);

        REQUIRE(exprs != std::nullopt);
        CHECK(exprs->size() == 2);
        // fmt::println("Expr 0: {}", (*exprs)[0].toJson().dump());
        // fmt::println("Expr 1: {}", (*exprs)[1].toJson().dump());
        CHECK((*exprs)[0].path.to_string() == "/msg");
        CHECK((*exprs)[1].path.to_string() == "/x");
        CHECK(*(*exprs)[1].op == Expr::Op::eq);
        CHECK(*(*exprs)[1].rhs == Value(2));
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
        // fmt::println("");
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
            // fmt::println("all bitsets: {}", a.bitsets);
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            // fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            // fmt::println("{} {}", keyBitSet, expected);
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
        // fmt::println("");
        CHECK(a.lines == lines);

        {
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            expected.push_back(true);
            expected.push_back(false);
            // fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            expected.push_back(false);
            expected.push_back(true);
            // fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
    }

    SUBCASE("Invalid: Gap") {
        // fmt::println("");
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
            // fmt::println("all bitsets: {}", a.bitsets);
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
            // fmt::println("Expected Error: {}", e.what());
            CHECK(true);
        }
    }

    SUBCASE("overlapping") {
        // fmt::println("");
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
            // fmt::println("all bitsets: {}", a.bitsets);
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            // fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            // fmt::println("{} {}", keyBitSet, expected);
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
        // fmt::println("");
        CHECK(a.lines == lines);

        {
            BitSet keyBitSet = a.bitsets[Path("msg").frontHash];
            BitSet expected;
            expected.push_back(true);
            expected.push_back(false);
            expected.push_back(false);
            // fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
        {
            BitSet keyBitSet = a.bitsets[Path("count").frontHash];
            BitSet expected;
            expected.push_back(false);
            expected.push_back(true);
            expected.push_back(true);
            // fmt::println("{} {}", keyBitSet, expected);
            CHECK(keyBitSet == expected);
        }
    }
}

TEST_CASE("Run Query") {
    // fmt::println("\nRun Query Test");
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

    SUBCASE("msg only") {
        // fmt::println("\nRun Query Test > msg only");
        auto maybeQuery = Query::parse("msg");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        auto   qr    = runQueryOnIndex(index, std::move(query));

        {
            CHECK(qr != std::nullopt);
            CHECK(qr->lines.size() == 1);
            std::vector<std::string> expected = {"msg: \"from the future\""};
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("count only") {
        // fmt::println("\nRun Query Test > count only");
        auto maybeQuery = Query::parse("count");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        auto   qr    = runQueryOnIndex(index, std::move(query));

        {
            CHECK(qr != std::nullopt);
            std::vector<std::string> expected = {"count: 23", "count: 21"};
            CHECK(qr->lines.size() == 2);
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("count > 22 predicate") {
        // fmt::println("\nRun Query Test > count > 22");
        auto maybeQuery = Query::parse("count > 22");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        auto   qr    = runQueryOnIndex(index, std::move(query));

        {
            CHECK(qr != std::nullopt);
            std::vector<std::string> expected = {"count: 23"};
            CHECK(qr->lines.size() == 1);
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("msg and count == 21") {
        // //fmt::println("\nRun Query Test > msg, count == 21");
        auto maybeQuery = Query::parse("msg, count == 21");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        auto   qr    = runQueryOnIndex(index, std::move(query));

        {
            CHECK(qr != std::nullopt);
            CHECK(qr->lines.size() == 1);
            std::vector<std::string> expected = {
                "msg: \"from the future\",  count: 21"
            };
            fmt::println("qr->lines {}", qr->lines);
            fmt::println("expected  {}", expected);
            CHECK(qr->lines == expected);
        }
    }

    SUBCASE("wildcard") {
        // fmt::println("\nRun Query Test > *");
        auto maybeQuery = Query::parse("*");
        CHECK(maybeQuery != std::nullopt);
        Query& query = *maybeQuery;
        auto   qr    = runQueryOnIndex(index, std::move(query));

        {
            CHECK(qr != std::nullopt);
            CHECK(qr->lines.size() == 2);
            std::vector<std::string> expected = {
                "count: 23", "msg: \"from the future\",  count: 21"
            };
            CHECK(qr->lines == expected);
        }
    }
}

template <typename T, typename Func>
void print(const std::vector<T>& v, Func f) {
    fmt::println("[");
    for (int i = 0; i < v.size(); ++i) {
        fmt::print("\t{}", f(v[i]));
        if (i + 1 != v.size()) {
            fmt::println(", ");
        } else {
            fmt::println("");
        }
    }
    fmt::println("]");
}

void printJson(const std::vector<json>& v) {
    print(v, [](const json& j) { return j.dump(); });
}

TEST_CASE("Ingestor") {
    // set up tmp file with data
    std::vector<json> sampleData = {
        {{"msg", "first message"}, {"count", 1}, {"tag", 5}},
        {{"msg", "hi"}, {"count", 2}, {"tag", 3}},
        {{"msg", "hi"}, {"count", 3}},
        {{"msg", "4th message"}, {"count", 4}, {"tag", 5}},
    };

    // create tmp file and write sampleData to it
    std::string   tmpFilename = "tmpfile";
    std::ofstream writeFile(tmpFilename);

    auto writeData = [&](const std::vector<json>& objs) {
        for (const auto& obj : objs) {
            writeFile << obj.dump() << std::endl;
        }
        writeFile.flush();
    };
    writeData(sampleData);

    folly::MPMCQueue<Msg> queue(10);  // TODO: 1 should work, right?
    std::atomic<bool>     shutdownFlag(false);

    auto waitForResponse = [&](std::function<void(Index&)> f) {
        Msg msg;
        queue.blockingRead(msg);
        std::visit(
            overloaded{
                [](StopSignal&) {},
                [&](Index& index) { f(index); },
                [&](Query& query) {
                    fmt::println("Received `Query` branch in Ingestor test");
                    CHECK(false);
                },
            },
            msg
        );
    };

    fmt::println("Spawning Ingestor...");
    std::fstream readHandle(tmpFilename, std::ios::in);
    std::thread  ingestor = spawnIngestor(queue, readHandle, shutdownFlag);
    fmt::println("Ingestor Spawned");

    {
        waitForResponse([&](Index& index) {
            fmt::println("Checking incoming Index");
            CHECK(index.start_idx == 0);
            CHECK(index.lines.size() == 4);
            printJson(index.lines);
            CHECK(index.lines == sampleData);
        });
    }

    {
        sampleData = {
            {{"msg", "another one"}},
            {{"count", 124}},
        };
        writeData(sampleData);

        waitForResponse([&](Index& index) {
            fmt::println("2nd index sent");
            CHECK(index.start_idx == 4);
            CHECK(index.lines.size() == 2);
            printJson(index.lines);
            CHECK(index.lines == sampleData);
        });
    }

    {
        sampleData = {
            {{"msg", "another one"}},  //
            {{"count", 124}},          //
            {{"count", 125}},          //
            {{"count", 126}},          //
            {{"count", 127}},          //
        };
        writeData(sampleData);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<json> sampleData2 = {
            {{"count", 128}},
        };
        writeData(sampleData2);

        waitForResponse([&](Index& index) {
            fmt::println("3rd index sent");
            CHECK(index.start_idx == 6);
            CHECK(index.lines.size() == 5);
            printJson(index.lines);

            CHECK(index.lines == sampleData);
        });
        waitForResponse([&](Index& index) {
            fmt::println("4th index sent");
            CHECK(index.start_idx == 11);
            CHECK(index.lines.size() == 1);
            printJson(index.lines);

            CHECK(index.lines == sampleData2);
        });
    }

    shutdownFlag.store(true);
    ingestor.join();
    std::filesystem::remove(tmpFilename);
    fmt::println("Ingestor successfully shutdown");
}

TEST_CASE("QueryService") {
    auto make = [](std::vector<json>& lines) {
        Index ind;
        for (auto& line : lines) {
            updateIndex(ind, std::move(line));
        }
        return std::move(ind);
    };

    folly::MPMCQueue<Msg> channel(10);  // TODO: 1 should work, right?
    int                   onUpdateCounter = 0;
    std::function<void()> onUpdate        = [&]() { ++onUpdateCounter; };
    folly::Synchronized<QueryResult> queryResult;
    auto check = [&queryResult](int seq, std::vector<std::string> lines) {
        auto qr = queryResult.rlock();
        fmt::println("[Check] Actual: {}, Expected: {}", qr->query.seq, seq);
        fmt::println("[Check] Actual: {}, Expected: {}", qr->lines, lines);
        CHECK(qr->query.seq == seq);
        CHECK(qr->lines.size() == lines.size());
        CHECK(qr->lines == lines);
    };

    std::thread       join = spawnQueryService(channel, queryResult, onUpdate);
    std::vector<json> sampleData = {
        {{"msg", "first message"}, {"count", 1}, {"tag", 5}},
        {{"msg", "hi"}, {"count", 2}, {"tag", 3}},
        {{"msg", "hi"}, {"count", 3}},
        {{"msg", "4th message"}, {"count", 4}, {"tag", 5}},
    };

    check(0, {});  // nothing yet
    channel.blockingWrite(std::move(make(sampleData)));
    check(0, {});  // nothing yet
    channel.blockingWrite(std::move(*Query::parse("tag", 1)));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(1, {"tag: 5", "tag: 3", "tag: 5"});  // results of `tag` query

    channel.blockingWrite(std::move(*Query::parse("tag == 5", 2)));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(2, {"tag: 5", "tag: 5"});  // results of `tag` query

    fmt::println("Sending stop signal...");
    channel.blockingWrite(StopSignal{});
    fmt::println("Stop signal sent");
    join.join();
    fmt::println("QS joined");
}
