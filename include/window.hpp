#pragma once

#include "geometry.hpp"

#include <map>

#include <boost/cobalt/promise.hpp>

namespace nvim {
class Graphics;

class Window {
    const int id_{};
    Point pos_{};
    Size offsets_{};
    Size size_{};
    std::pair<int, int> visible_{};

    static std::map<int, Window> cache_;

    Window(int id, Point pos, Size offsets, Size size, std::pair<int, int> visible);

    auto update(Graphics& api) -> boost::cobalt::promise<bool>;

public:
    static auto get(Graphics& api, int win) -> boost::cobalt::promise<Window>;
    static auto invalidate(int win) -> void;
    static auto update(Graphics& api, int win) -> boost::cobalt::promise<void>;

    auto position() const -> Point;
    auto size() const -> Size;
    auto id() const -> int;
    auto visibility() const -> std::pair<int, int>;
};

} // namespace nvim
