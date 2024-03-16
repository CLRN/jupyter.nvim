#include "window.hpp"
#include "api.hpp"
#include "geometry.hpp"
#include "graphics.hpp"

#include <spdlog/spdlog.h>

namespace nvim {

std::map<int, Window> Window::cache_;

Window::Window(int id, Point pos, Size offsets, Size size, std::pair<int, int> visible)
    : id_{id}
    , pos_{std::move(pos)}
    , offsets_{std::move(offsets)}
    , size_{std::move(size)}
    , visible_{std::move(visible)} {}

auto Window::get(Graphics& api, int win) -> boost::cobalt::promise<Window> {
    auto it = cache_.find(win);
    if (it == cache_.end()) {
        auto [terminal_pos, nvim_pos, w, h, visible] = co_await boost::cobalt::join(
            api.position(win), api.api().nvim_win_get_position(win), api.api().nvim_win_get_width(win),
            api.api().nvim_win_get_height(win), api.visible_area());
        auto offsets = Size{.w = terminal_pos.x - nvim_pos.x, .h = terminal_pos.y - nvim_pos.y};

        // convert to zero-based offsets, only need first line(?)
        visible.first -= 1;
        visible.second = visible.first + h;

        it = cache_
                 .emplace(win,
                          Window{
                              win,
                              terminal_pos,
                              offsets,
                              Size{.w = w, .h = h},
                              visible,
                          })
                 .first;

        spdlog::info("Detected window {}, terminal position: {}, nvim window offsets: {}, window area offsets: {}, "
                     "size: {}, visible {}-{}",
                     win, terminal_pos, nvim_pos, offsets, it->second.size(), visible.first, visible.second);
    }
    co_return it->second;
}

auto Window::invalidate(int win) -> void {
    cache_.erase(win);
}

auto Window::update(Graphics& api, int win) -> boost::cobalt::promise<void> {
    const auto it = cache_.find(win);
    if (it != cache_.end()) {
        co_await it->second.update(api);
    }
}

auto Window::update(Graphics& api) -> boost::cobalt::promise<bool> {
    const auto out = co_await api.api().nvim_exec2("lua print(vim.fn['line']('w0'))", {{"output", true}});
    const auto prev = visible_.first;
    visible_.first = std::stoi(out.find("output")->second.as_string()) - 1;
    visible_.second = visible_.first + size_.h;
    co_return prev == visible_.first;
}

auto Window::position() const -> Point {
    return pos_;
}

auto Window::size() const -> Size {
    return size_;
}

auto Window::id() const -> int {
    return id_;
}

auto Window::visibility() const -> std::pair<int, int> {
    return visible_;
}

} // namespace nvim
