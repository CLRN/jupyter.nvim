#pragma once

#include <boost/cobalt/promise.hpp>

#include <string>

namespace nvim {
class Api;

class RemoteGraphics {
    std::ofstream ofs_;

public:
    RemoteGraphics(std::string tty);
    auto stream() -> std::ostream&;
};

class Graphics {
    Api& api_;

public:
    Graphics(Api& api);

    auto screen_size(int attempts = 5) -> boost::cobalt::promise<std::pair<int, int>>;
    auto get_tty(nvim::Api& api) -> boost::cobalt::promise<std::string>;
    auto remote() -> boost::cobalt::promise<RemoteGraphics>;
};

} // namespace nvim
