#pragma once

#include <map>
#include <utility>

#include <boost/cobalt/promise.hpp>

namespace nvim {
class Graphics;

class Window {
    const int id_{};
    std::pair<int, int> pos_{};
    std::pair<int, int> offsets_{};
    std::pair<int, int> size_{};

    static std::map<std::pair<int, int>, Window> cache_;

    Window(int id, std::pair<int, int> pos, std::pair<int, int> offsets, std::pair<int, int> size);

public:
    static auto get(Graphics& api, int buf, int win) -> boost::cobalt::promise<Window>;
    static auto invalidate(int buf, int win) -> void;

    auto position() const -> std::pair<int, int>;
    auto size() const -> std::pair<int, int>;
    auto id() const -> int;
};

} // namespace nvim
