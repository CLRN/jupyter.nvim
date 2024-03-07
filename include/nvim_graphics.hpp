#pragma once

#include <boost/cobalt/promise.hpp>

#include <string>
#include <utility>

namespace nvim {
class Api;

class Graphics {
    Api& api_;
    const int retry_count_{};

    std::string tty_;
    std::ofstream ofs_;
    std::pair<int, int> screen_size_{};
    std::pair<int, int> terminal_size_{};
    std::pair<double, double> cell_size_{};

    auto run_lua_io(std::string_view data) -> boost::cobalt::promise<std::string>;

public:
    Graphics(Api& api, int retry_count = 5);

    auto api() -> Api&;

    auto init() -> boost::cobalt::promise<void>;
    auto update() -> boost::cobalt::promise<void>;

    // returns height and width
    auto screen_size() -> boost::cobalt::promise<std::pair<int, int>>;
    auto terminal_size() -> std::pair<int, int>;
    auto cell_size() -> std::pair<double, double>;

    // returns row and col
    auto position(int win_id) -> boost::cobalt::promise<std::pair<int, int>>;

    auto get_tty(nvim::Api& api) -> boost::cobalt::promise<std::string>;

    auto stream() -> std::ostream&;
};

} // namespace nvim
