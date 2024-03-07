#include "window.hpp"
#include "nvim.hpp"
#include "nvim_graphics.hpp"
#include "spdlog/spdlog.h"

namespace nvim {

std::map<std::pair<int, int>, Window> Window::cache_;

Window::Window(int id, std::pair<int, int> pos, std::pair<int, int> offsets, std::pair<int, int> size)
    : id_{id}
    , pos_{std::move(pos)}
    , offsets_{std::move(offsets)}
    , size_{std::move(size)} {}

auto Window::get(Graphics& api, int buf, int win) -> boost::cobalt::promise<Window> {
    auto it = cache_.find(std::make_pair(buf, win));
    if (it == cache_.end()) {
        const auto [terminal_pos, nvim_pos, w, h] =
            co_await boost::cobalt::join(api.position(win), api.api().nvim_win_get_position(win),
                                         api.api().nvim_win_get_width(win), api.api().nvim_win_get_height(win));
        it = cache_
                 .emplace(std::make_pair(buf, win), Window{win, terminal_pos,
                                                           std::make_pair(terminal_pos.first - nvim_pos.front(),
                                                                          terminal_pos.second - nvim_pos.back()),
                                                           std::make_pair(h, w)})
                 .first;

        spdlog::info("Detected window {}, terminal position: {}x{}, nvim position: {}x{}, size: {}x{}", win,
                     it->second.pos_.first, it->second.pos_.second, it->second.offsets_.first,
                     it->second.offsets_.second, h, w);
    }
    co_return it->second;
}

auto Window::invalidate(int buf, int win) -> void {
    cache_.erase(std::make_pair(buf, win));
}

auto Window::position() const -> std::pair<int, int> {
    return pos_;
}

auto Window::size() const -> std::pair<int, int> {
    return size_;
}

auto Window::id() const -> int {
    return id_;
}

} // namespace nvim
