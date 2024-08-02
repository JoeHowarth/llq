#include <fmt/core.h>

#include <chrono>
#include <ftxui/component/component.hpp>       // for Input, Renderer, Vertical
#include <ftxui/component/component_base.hpp>  // for Component, ComponentBase
#include <ftxui/component/component_options.hpp>  // for InputOption
#include <ftxui/component/event.hpp>  // for Event, Event::ArrowDown, Event::ArrowUp, Event::End, Event::Home, Event::PageDown, Event::PageUp
#include <ftxui/component/receiver.hpp>  // for Sender / Receiver channel
#include <ftxui/component/screen_interactive.hpp>  // for Component, ScreenInteractive
#include <ftxui/dom/elements.hpp>  // for text, hbox, separator, Element, operator|, vbox, border
#include <ftxui/dom/linear_gradient.hpp>  // for LinearGradient
#include <ftxui/dom/node.hpp>             // for Node
#include <ftxui/screen/color.hpp>  // for Color, Color::White, Color::Red, Color::Blue, Color::Black, Color::GrayDark, ftxui
#include <ftxui/util/ref.hpp>      // for Ref
#include <functional>              // for function
#include <memory>                  // for allocator, __shared_ptr_access
#include <ranges>
#include <string>  // for char_traits, operator+, string, basic_string
#include <thread>
#include <utility>  // for move

#include "lib.h"
#include "logging.h"
#include "parser.h"

// TODO:
// - [ ] Make scrollable lines work
// - [X] More recent log appears at bottom
// - [ ] Stream new changes to file
// - [ ] Fix lag

// TODO: Build in-memory file repr that is fast to search
// - 

struct Query {
    long              seq;
    std::string       str;
    std::vector<Expr> exprs;
};

struct QueryResult {
    Query                    query;
    std::vector<json>        jsonLines;
    std::vector<std::string> lines;
};

void queryService(
    ftxui::ScreenInteractive& screen,
    ftxui::Receiver<Query>    rcvr,
    QueryResult&              queryResult,
    const std::string&        fname
) {
    auto sender = rcvr->MakeSender();
    // std::thread notifier([&queryResult, sender = std::move(sender)]() {
    //     while (true) {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(300));
    //         if (queryResult.query.str.size() > 0) {
    //             sender->Send(queryResult.query);
    //         }
    //     }
    // });

    int  maxMatches  = -1;
    long lastSeenSeq = 0;
    // TODO: screen.dimy() is likely not thread safe to read AND not
    // initialized correctly. This hack reads it from the main thread hopefully
    // after initialization. Replace with a better solution
    screen.Post([&maxMatches, &screen]() { maxMatches = screen.dimy() - 3; });
    Query query;
    while (rcvr->Receive(&query)) {
        if (lastSeenSeq > query.seq) {
            Log::info(
                "Query has old seq",
                {{"seq", query.seq}, {"lastSeen", lastSeenSeq}}
            );
            continue;
        }

        if (maxMatches == -1) {
            // FIXME: if still not initialized, pray
            maxMatches = screen.dimy() - 3;
        }

        std::vector<json> results =
            runQuery(query.exprs, fname, maxMatches, 10000);
        if (results.size() > 0) {
            // guard against `query` variable being overwritten before task runs
            // on ui thread
            std::string queryStr = query.str;
            auto        lines    = formatResults(results);
            QueryResult toMove   = {
                  .query     = std::move(query),
                  .jsonLines = std::move(results),
                  .lines     = std::move(lines),
            };
            screen.Post([&screen, &queryResult, moved = std::move(toMove),
                         &maxMatches]() mutable {
                queryResult = std::move(moved);
                // update maxMatches to account for resizes
                maxMatches = screen.dimy() - 3;

                screen.PostEvent(ftxui::Event::Custom);
            });
        }
    }
}

void ui(const std::string& fname) {
    using namespace ftxui;

    auto                   screen = ScreenInteractive::Fullscreen();
    std::string            rawQuery;
    ftxui::Receiver<Query> rcvr        = MakeReceiver<Query>();
    auto                   querySender = rcvr->MakeSender();
    QueryResult            queryResult = {{0, "", {}}, {}, {}};
    long                   seq         = 0;
    std::thread            joinQueryService([&]() {
        queryService(screen, std::move(rcvr), queryResult, fname);
    });

    InputOption style = InputOption::Default();
    style.transform   = [](InputState state) {
        if (state.is_placeholder) {
            state.element |= dim;
        }

        return state.element;
    };
    Component exprsInput = Input(&rawQuery, "", style);

    exprsInput |= CatchEvent([&](Event event) {
        json tag = {{"tag", "CatchEvent"}};
        if (!event.is_character() && ftxui::Event::Character('\n') != event) {
            Log::info("not a character event or [Enter]", tag);
            return false;
        }
        Log::info(
            "char event",
            Log::merge(
                {{"char", event.character()}, {"rawQuery", rawQuery}}, tag
            )
        );

        if (ftxui::Event::Character('\n') != event) {
            // backspace / delete
            if (event == Event::Backspace) {
                Log::info("Backspace", tag);
            }
            if (event == Event::Delete) {
                Log::info("Delete", tag);
            }

            rawQuery.push_back(event.character()[0]);

            if (auto parsed = parser::parseExprs(rawQuery)) {
                Log::info("Parse succeeded", tag);
                // send query to QueryService
                querySender->Send(
                    {.seq = seq++, .str = rawQuery, .exprs = *parsed}
                );
            } else {
                Log::info("Parse failed", tag);
            }

            rawQuery.pop_back();
            return false;
        }

        // handle backsapce (delete on macos keyboad)
        rawQuery.clear();
        Log::info("Character is [Enter]", tag);
        return true;
    });

    // The component tree:
    auto component = Container::Vertical({
        exprsInput,
    });

    // Tweak how the component tree is rendered:
    auto renderer = Renderer(component, [&] {
        std::vector<Element> lines;
        lines.reserve(queryResult.lines.size());
        for (auto& line : std::ranges::reverse_view(queryResult.lines)) {
            lines.push_back(text(line));
        }

        return vbox({
            vbox(std::move(lines)) | yflex_grow,                       //
            filler(),                                                  //
            separator(),                                               //
            hbox({text("Query   :> "), exprsInput->Render()}),         //
            hbox({text("Current :> "), text(queryResult.query.str)}),  //
        });
    });

    screen.Loop(renderer);
}

int main(int argc, char** argv) {
    Log::init("log.json");
    Log::sendToCout = false;
    Log::info("Hello from Live Log Query (llq)!");

    std::string fname = argc > 1 ? argv[1] : "dummy_log.json";
    ui(fname);
}
