#pragma once

#include <folly/MPMCQueue.h>
#include <folly/Synchronized.h>

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
#include <string>   // for char_traits, operator+, string, basic_string
#include <utility>  // for move

#include "logging.h"
#include "parser.h"
#include "types.h"

std::function<void()>
ui(folly::MPMCQueue<Msg>&            queryService,
   folly::Synchronized<QueryResult>& queryResult) {
    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen();
    auto threadSafeReRender = [&screen]() {
        screen.Post([&screen]() { screen.PostEvent(ftxui::Event::Custom); });
    };
    return threadSafeReRender;
    /*
    std::string            rawQuery;
    ftxui::Receiver<Query> rcvr = MakeReceiver<Query>();
    long                   seq  = 0;

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
                // TODO: which write call?
                queryService.write(Query(seq++, rawQuery, std::move(*parsed)));
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
        auto                 qr = queryResult.rlock();
        lines.reserve(qr->lines.size());
        for (const auto& line : std::ranges::reverse_view(qr->lines)) {
            lines.push_back(text(line));
        }

        return vbox({
            vbox(std::move(lines)) | yflex_grow,                //
            filler(),                                           //
            separator(),                                        //
            hbox({text("Query   :> "), exprsInput->Render()}),  //
            hbox({text("Current :> "), text(qr->query.str)}),   //
        });
    });

    screen.Loop(renderer);
    */
}
