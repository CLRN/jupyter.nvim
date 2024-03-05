#include "kitty.hpp"
#include "nvim_graphics.hpp"

#include <boost/beast/core/detail/base64.hpp>

#include <utility>

namespace kitty {

class Command {
    nvim::RemoteGraphics& nvim_;
    int level_{};

public:
    template <typename... T>
    Command(nvim::RemoteGraphics& nvim, T... args)
        : nvim_{nvim} {
        nvim_.stream() << "\x1b_G";

        if constexpr (sizeof...(args)) {
            add(std::move(args)...);
        }
    }

    template <typename K, typename V, typename... A>
    void add(K k, V v, A... args) {
        nvim_.stream() << (level_++ ? "," : "") << k << "=" << v;

        if constexpr (sizeof...(args)) {
            add(std::move(args)...);
        }
    }

    ~Command() {
        nvim_.stream() << "\x1b\\" << std::flush;
    }
};

Cursor::~Cursor() {
    nvim_.stream() << "\0338" << std::flush; // restore pos
}

Cursor::Cursor(nvim::RemoteGraphics& nvim, int x, int y)
    : nvim_{nvim}
    , x_{x}
    , y_{y} {
    nvim_.stream() << "\0337";                         // save pos
    nvim_.stream() << "\033[" << y << ";" << x << "f"; // move
}

Image::Image(nvim::RemoteGraphics& nvim, std::string content)
    : nvim_{nvim}
    , png_{std::move(content)} {

    static int id_cnt_{};
    id_ = ++id_cnt_;

    std::vector<char> encoded(boost::beast::detail::base64::encoded_size(png_.size()));
    boost::beast::detail::base64::encode(encoded.data(), png_.data(), png_.size());

    std::vector<std::string_view> chunks;
    constexpr std::size_t chunk_size = 4096;
    for (std::size_t i{}; i < encoded.size(); i += chunk_size) {
        chunks.emplace_back(&encoded[i], std::min(chunk_size, encoded.size() - i));
    }

    for (std::size_t i{}; i < chunks.size(); ++i) {
        Command c{nvim_, 'q', 2};
        if (!i) {
            c.add('a', 't', 'f', 100, 'C', 1, 'i', id_);
        }

        if (i < chunks.size() - 1) {
            c.add('m', 1);
        }

        nvim_.stream() << ";" << chunks[i];
    }
}

Image::Image(Image&& im)
    : nvim_{im.nvim_}
    , png_{std::move(im.png_)}
    , id_{std::exchange(im.id_, 0)} {}

Image::~Image() {
    if (id_) {
        Command c{nvim_, 'a', 'd', 'i', id_, 'q', 2};
    }
}

void Image::place(int x, int y, int id) {
    Cursor cursor{nvim_, x, y};
    Command command{nvim_, 'a', 'p', 'i', id_, 'p', id * 10000 + id_, 'q', 2};
}

void Image::clear(int id) {
    Command command{nvim_, 'a', 'd', 'd', 'i', 'i', id_, 'q', 2};
    if (id) {
        command.add('p', id * 10000 + id_);
    }
}

} // namespace kitty
