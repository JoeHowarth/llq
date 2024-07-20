#include <fmt/core.h>

#include <algorithm>                           // for max, min
#include <ftxui/component/component_base.hpp>  // for Component, ComponentBase
#include <ftxui/component/event.hpp>  // for Event, Event::ArrowDown, Event::ArrowUp, Event::End, Event::Home, Event::PageDown, Event::PageUp
#include <ftxui/dom/linear_gradient.hpp>  // for LinearGradient
#include <ftxui/screen/color.hpp>  // for Color, Color::White, Color::Red, Color::Blue, Color::Black, Color::GrayDark, ftxui
#include <functional>              // for function
#include <memory>                  // for allocator, __shared_ptr_access
#include <string>   // for char_traits, operator+, string, basic_string
#include <utility>  // for move

#include "ftxui/component/component.hpp"  // for Input, Renderer, Vertical
#include "ftxui/component/component_options.hpp"  // for InputOption
#include "ftxui/component/mouse.hpp"  // for Mouse, Mouse::WheelDown, Mouse::WheelUp
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/dom/deprecated.hpp"                // for text
#include "ftxui/dom/elements.hpp"  // for text, hbox, separator, Element, operator|, vbox, border
#include "ftxui/dom/node.hpp"         // for Node
#include "ftxui/dom/requirement.hpp"  // for Requirement
#include "ftxui/screen/box.hpp"       // for Box
#include "ftxui/util/ref.hpp"         // for Ref
#include "lib.h"
#include "logging.h"
#include "parser.h"

namespace ftxui {

class ScrollerBase : public ComponentBase {
   public:
    ScrollerBase(Component child) {
        Add(std::move(child));
    }

   private:
    Element Render() final {
        auto focused = Focused() ? focus : ftxui::select;
        auto style   = Focused() ? inverted : nothing;

        Element background = ComponentBase::Render();
        background->ComputeRequirement();
        size_ = background->requirement().min_y;
        return dbox({
                   std::move(background),
                   vbox({
                       text(L"") | size(HEIGHT, EQUAL, selected_),
                       text(L"") | style | focused,
                   }),
               }) |
               vscroll_indicator | yframe | yflex | reflect(box_);
    }

    bool OnEvent(Event event) final {
        if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y))
            TakeFocus();

        int selected_old = selected_;
        if (event == Event::ArrowUp || event == Event::Character('k') ||
            (event.is_mouse() && event.mouse().button == Mouse::WheelUp)) {
            selected_--;
        }
        if ((event == Event::ArrowDown || event == Event::Character('j') ||
             (event.is_mouse() && event.mouse().button == Mouse::WheelDown))) {
            selected_++;
        }
        if (event == Event::PageDown) selected_ += box_.y_max - box_.y_min;
        if (event == Event::PageUp) selected_ -= box_.y_max - box_.y_min;
        if (event == Event::Home) selected_ = 0;
        if (event == Event::End) selected_ = size_;

        selected_ = std::max(0, std::min(size_ - 1, selected_));
        return selected_old != selected_;
    }

    bool Focusable() const final {
        return true;
    }

    int selected_ = 0;
    int size_     = 0;
    Box box_;
};

Component Scroller(Component child) {
    return Make<ScrollerBase>(std::move(child));
}
}  // namespace ftxui


// TODO: 
// - Make scrollable lines work
// - More recent log appears at bottom
// - Stream new changes to file 

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
    bool                    valid      = false;
    Component               exprsInput = Input(&rawQuery, "", style);

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
                if (runQuery(*parsed, fname).size() > 0) {
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

    // Tweak how the component tree is rendered:
    auto renderer = Renderer(component, [&] {
        auto                 filteredJsonLines = runQuery(exprs, fname);
        auto                 formattedLines = formatResults(filteredJsonLines);
        std::vector<Element> lines;
        lines.reserve(formattedLines.size());
        for (const auto& line : formattedLines) {
            lines.push_back(text(line));
        }

        auto lines_renderer =
            Renderer([&] { return vbox(std::move(lines)) | yflex_grow; });

        return vbox({
            ftxui::Scroller(lines_renderer)->Render(),             //
            filler(),                                              //
            separator(),                                           //
            hbox({text("Query   :> "), exprsInput->Render()}),     //
            hbox({text("Current :> "), text(lastNonEmptyQuery)}),  //
        });
    });

    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(renderer);
}

int main(int argc, char** argv) {
    Log::init("log.json");
    std::string fname = argc > 1 ? argv[1] : "dummy_log.json";

    Log::sendToCout = false;

    Log::info("Hello from Live Log Query (llq)!");

    ui(fname);
}
