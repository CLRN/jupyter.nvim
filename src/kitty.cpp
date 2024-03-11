#include "kitty.hpp"
#include "graphics.hpp"
#include "window.hpp"

#include <boost/beast/core/detail/base64.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <sys/socket.h>
#include <utility>

namespace kitty {

class Command {
    nvim::Graphics& nvim_;
    int level_{};

public:
    template <typename... T>
    Command(nvim::Graphics& nvim, T... args)
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

Cursor::Cursor(nvim::Graphics& nvim, int x, int y)
    : nvim_{nvim}
    , x_{x}
    , y_{y} {
    nvim_.stream() << "\0337";                         // save pos
    nvim_.stream() << "\033[" << y << ";" << x << "f"; // move
}

Image::Image(nvim::Graphics& nvim)
    : nvim_{nvim}
    , image_{} {
    static int id_cnt_{};
    id_ = ++id_cnt_;
}

auto Image::send() {
    spdlog::debug("[{}] Encoding image to png, size {}", id_, image_.total() * image_.elemSize());

    std::vector<std::uint8_t> content;
    cv::imencode(".png", image_, content, {cv::IMWRITE_PNG_COMPRESSION});
    spdlog::debug("[{}] Encoded image to png, size {}", id_, content.size());

    std::vector<char> encoded(boost::beast::detail::base64::encoded_size(content.size()));
    boost::beast::detail::base64::encode(encoded.data(), content.data(), content.size());

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

    spdlog::debug("[{}] Sent image to neovim, size {}", id_, encoded.size());
}

auto Image::load(const std::string& path) -> void {
    spdlog::debug("[{}] Reading image from {}", id_, path);
    image_ = cv::imread(path, cv::IMREAD_UNCHANGED);
    send();
}

auto Image::load(const std::vector<std::uint8_t>& content) -> void {
    spdlog::debug("[{}] Reading image content size {}", id_, content.size());
    image_ = cv::imdecode(content, cv::IMREAD_UNCHANGED);
    send();
}

Image::Image(Image&& im)
    : nvim_{im.nvim_}
    , id_{std::exchange(im.id_, 0)}
    , image_{std::move(im.image_)} {}

Image::~Image() {
    if (id_) {
        spdlog::debug("[{}] Deleted image from neovim", id_);
        Command command{nvim_, 'a', 'd', 'd', 'i', 'i', id_, 'q', 2};
    }
}

auto Image::area(const nvim::Window& win) const -> nvim::Size {
    const auto img_size = nvim::Size{.w = image_.size[1], .h = image_.size[0]};
    const auto cell_size = nvim_.cell_size();
    const auto win_size = win.size();
    const auto win_size_px = nvim::Size{.w = cell_size.w * win_size.w, .h = cell_size.h * win_size.h};

    const auto ratio = double(img_size.w) / img_size.h;

    const auto placement_size = (img_size.h > win_size_px.h || img_size.w > win_size_px.w)
                                    ? nvim::Size{.w = std::min(win_size.w, int(double(win_size.h) * ratio)),
                                                 .h = std::min(win_size.h, int(double(win_size.w) / ratio))}
                                    : nvim::Size{.w = img_size.w / cell_size.w, .h = img_size.h / cell_size.h};

    return placement_size;
}

auto Image::place(nvim::Point where, const nvim::Window& win) const -> nvim::Size {
    where.x += win.position().x;
    where.y += win.position().y;

    const auto placement_size = area(win);

    spdlog::debug("[{}] Placing image with id {} to {} with size: {}", id_, win.id() * 10000 + id_, where,
                  placement_size);

    Cursor cursor{nvim_, where.x, where.y};
    Command command{
        nvim_, 'a', 'p', 'i', id_, 'p', win.id() * 10000 + id_, 'q', 2, 'c', placement_size.w, 'r', placement_size.h};

    return placement_size;
}

auto Image::clear(int id) -> void {
    Command command{nvim_, 'a', 'd', 'd', 'i', 'i', id_, 'q', 2};
    if (id) {
        spdlog::debug("[{}] Clearing image with id {}", id_, id * 10000 + id_);
        command.add('p', id * 10000 + id_);
    }
}

} // namespace kitty
