#include <fmt/core.h>

#include <ftxui/dom/linear_gradient.hpp>  // for LinearGradient
#include <ftxui/screen/color.hpp>  // for Color, Color::White, Color::Red, Color::Blue, Color::Black, Color::GrayDark, ftxui
#include <functional>              // for function
#include <memory>                  // for allocator, __shared_ptr_access
#include <string>  // for char_traits, operator+, string, basic_string

#include "ftxui/component/component.hpp"       // for Input, Renderer, Vertical
#include "ftxui/component/component_base.hpp"  // for ComponentBase
#include "ftxui/component/component_options.hpp"  // for InputOption
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/dom/elements.hpp"  // for text, hbox, separator, Element, operator|, vbox, border
#include "ftxui/util/ref.hpp"  // for Ref
#include "lib.h"
#include "logging.h"
#include "parser.h"

void ui() {
    using namespace ftxui;

    InputOption style = InputOption::Default();
    style.transform   = [](InputState state) {
        // state.element |= borderEmpty;

        if (state.is_placeholder) {
            state.element |= dim;
        }

        state.element |= bgcolor(Color::White);
        state.element |= color(Color::Black);

        return state.element;
    };

    std::vector<Expr>       exprs;
    std::string             rawQuery;
    std::deque<std::string> history;
    Component               exprsInput = Input(&rawQuery, "Query:> ", style);

    exprsInput |= CatchEvent([&](Event event) {
        if (ftxui::Event::Character('\n') != event) {
            return false;
        }
        if (auto parsed = parser::parseExprs(rawQuery)) {
            exprs = std::move(*parsed);
            history.push_back(rawQuery);
            rawQuery.clear();
        }
        return true;
    });

    // exprs_input |= CatchEvent([&](const Event& event) {
    //     if (event.is_character()) {
    //         if (auto parsed = parser::parseExprs(event.character())) {
    //             exprs = *parsed;
    //             return true;
    //         }
    //     }
    //     return false;
    // });

    // The component tree:
    auto component = Container::Vertical({
        exprsInput,
    });

    // Tweak how the component tree is rendered:
    auto renderer = Renderer(component, [&] {
        auto                 filteredJsonLines = runQuery(exprs, "dummy_log.json");
        auto                 formattedLines = formatResults(filteredJsonLines);
        std::vector<Element> lines;
        lines.reserve(formattedLines.size());
        for (const auto& line : formattedLines) {
            lines.push_back(text(line));
        }
        std::ranges::reverse(lines);

        return vbox({
            vbox(std::move(lines)),
            exprsInput->Render(),  //
            text(rawQuery),        //
        });
    });

    auto screen = ScreenInteractive::TerminalOutput();
    screen.Loop(renderer);
}

int main() {
    Log::init("log.json");
    Log::sendToCout = false;

    Log::info("Hello from Live Log Query (llq)!");

    Log::info("hi", {{"tick", 2}});
    Log::info("bye", {{"thing", {1, 2, 3}}});

    ui();

    // std::deque<std::string> rawQueries;
    // while (true) {
    //     std::string rawQuery;
    //     fmt::print("> ");
    //     std::getline(std::cin, rawQuery);
    //     bool success = runQuery(rawQuery, "log.json");
    //
    //     rawQueries.push_back(std::move(rawQuery));
    // }
}
