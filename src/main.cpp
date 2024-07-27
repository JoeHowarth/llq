#include <fmt/core.h>

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

void ui(const std::string& fname) {
    using namespace ftxui;

    InputOption style = InputOption::Default();
    style.transform   = [](InputState state) {
        if (state.is_placeholder) {
            state.element |= dim;
        }

        return state.element;
    };

    std::vector<Expr>       exprs;
    std::string             rawQuery;
    std::string             lastNonEmptyQuery;
    std::deque<std::string> history;
    bool                    valid        = false;
    Component               exprsInput   = Input(&rawQuery, "", style);
    auto                    exprReceiver = MakeReceiver<std::vector<Expr>>();
    auto                    exprSender   = exprReceiver->MakeSender();

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
            rawQuery.push_back(event.character()[0]);
            if (auto parsed = parser::parseExprs(rawQuery)) {
                Log::info("Parse succeeded", tag);
                if (runQuery(*parsed, fname, 1).size() > 0) {
                    Log::info("Query is non-empty", tag);
                    valid             = true;
                    exprs             = std::move(*parsed);
                    lastNonEmptyQuery = rawQuery;
                }
            } else {
                Log::info("Parse failed", tag);
            }
            rawQuery.pop_back();
            return false;
        }
        history.push_back(rawQuery);
        rawQuery.clear();
        Log::info("Character is [Enter]", tag);
        return true;
    });

    // The component tree:
    auto component = Container::Vertical({
        exprsInput,
    });

    auto screen = ScreenInteractive::Fullscreen();

    // Tweak how the component tree is rendered:
    auto renderer = Renderer(component, [&] {
        auto filteredJsonLines =
            runQuery(exprs, fname, screen.dimy() - 3, 100000);
        auto formattedLines = formatResults(filteredJsonLines);

        std::vector<Element> lines;
        lines.reserve(formattedLines.size());
        for (const auto& line : formattedLines) {
            lines.push_back(text(line));
        }

        return vbox({
            vbox(std::move(lines)) | yflex_grow,                   //
            filler(),                                              //
            separator(),                                           //
            hbox({text("Query   :> "), exprsInput->Render()}),     //
            hbox({text("Current :> "), text(lastNonEmptyQuery)}),  //
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
