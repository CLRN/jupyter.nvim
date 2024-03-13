#pragma once

#include "geometry.hpp"

#include <boost/cobalt/promise.hpp>

#include <string>

namespace nvim {
class Api;

class Graphics {
    Api& api_;
    const int retry_count_{};

    std::string tty_;
    std::ofstream ofs_;
    Size screen_size_{};
    Size terminal_size_{};
    Size cell_size_{};

    auto run_lua_io(std::string_view data) -> boost::cobalt::promise<std::string>;

public:
    Graphics(Api& api, int retry_count = 5);

    auto api() -> Api&;

    auto init() -> boost::cobalt::promise<void>;
    auto update() -> boost::cobalt::promise<void>;

    // returns height and width
    auto screen_size() -> boost::cobalt::promise<Size>;
    auto terminal_size() -> Size;
    auto cell_size() -> Size;

    // returns first and last visible lines
    auto visible_area() -> boost::cobalt::promise<std::pair<int, int>>;

    // returns row and col
    auto position(int win_id) -> boost::cobalt::promise<Point>;

    auto get_tty() -> boost::cobalt::promise<std::string>;

    auto stream() -> std::ostream&;
};

} // namespace nvim
