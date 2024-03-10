#include "window.hpp"
#include "geometry.hpp"
#include "api.hpp"
#include "graphics.hpp"

#include <spdlog/spdlog.h>

namespace nvim {

std::map<int, Window> Window::cache_;

Window::Window(int id, Point pos, Size offsets, Size size)
    : id_{id}
    , pos_{std::move(pos)}
    , offsets_{std::move(offsets)}
    , size_{std::move(size)} {}

auto Window::get(Graphics& api, int win) -> boost::cobalt::promise<Window> {
    auto it = cache_.find(win);
    if (it == cache_.end()) {
        const auto [terminal_pos, nvim_pos, w, h] =
            co_await boost::cobalt::join(api.position(win), api.api().nvim_win_get_position(win),
                                         api.api().nvim_win_get_width(win), api.api().nvim_win_get_height(win));
        auto size = Size{.w = terminal_pos.x - nvim_pos.x, .h = terminal_pos.y - nvim_pos.y};
        it = cache_
                 .emplace(win,
                          Window{
                              win,
                              terminal_pos,
                              size,
                              Size{.w = w, .h = h},
                          })
                 .first;

        spdlog::info("Detected window {}, terminal position: {}, nvim position: {}, size: {}", win, terminal_pos,
                     nvim_pos, size);
    }
    co_return it->second;
}

auto Window::invalidate(int win) -> void {
    cache_.erase(win);
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

} // namespace nvim
