#pragma once

#include <boost/cobalt/promise.hpp>

#include <string>
#include <utility>

namespace nvim {
class Api;

class RemoteGraphics {
    std::ofstream ofs_;
    std::pair<int, int> screen_size_{};
    std::pair<int, int> terminal_size_{};

public:
    RemoteGraphics(std::string tty, std::pair<int, int> screen_size, std::pair<int, int> terminal_size);
    auto stream() -> std::ostream&;
    auto screen_size() const -> std::pair<int, int>;
    auto terminal_size() const -> std::pair<int, int>;
};

class Graphics {
    Api& api_;

public:
    Graphics(Api& api);

    // returns height and width
    auto screen_size(int attempts = 5) -> boost::cobalt::promise<std::pair<int, int>>;

    auto get_tty(nvim::Api& api) -> boost::cobalt::promise<std::string>;
    auto remote() -> boost::cobalt::promise<RemoteGraphics>;
};

} // namespace nvim
