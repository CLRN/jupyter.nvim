#include "kitty.hpp"
#include "nvim_graphics.hpp"

#include <boost/beast/core/detail/base64.hpp>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
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

Image::Image(nvim::RemoteGraphics& nvim, const std::string& path)
    : nvim_{nvim}
    , image_{} {
    static int id_cnt_{};
    id_ = ++id_cnt_;

    spdlog::debug("[{}] Reading image from {}", id_, path);
    image_ = cv::imread(path, cv::IMREAD_UNCHANGED);

    spdlog::debug("[{}] Encoding image to png, size {}", id_, image_.total() * image_.elemSize());

    std::vector<uint8_t> content;
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

void Image::place(int x, int y, int w, int h, int id) {
    const int hpx = image_.size[0];
    const int wpx = image_.size[1];
    const auto [screen_px_h, screen_px_w] = nvim_.screen_size();
    const auto [terminal_h, terminal_w] = nvim_.terminal_size();
    const auto cell_size_h = screen_px_h ? screen_px_h / terminal_h : 1;
    const auto cell_size_w = screen_px_w ? screen_px_w / terminal_w : 1;
    const auto win_size_px_h = cell_size_h * h;
    const auto win_size_px_w = cell_size_w * w;

    const auto ratio = double(wpx) / hpx;

    Cursor cursor{nvim_, x, y + 1};
    Command command{nvim_, 'a', 'p', 'i', id_, 'p', id * 10000 + id_, 'q', 2};

    spdlog::debug("[{}] Placing image with id {}", id_, id * 10000 + id_);

    // check if the image is possible to fit, specify rows and cols to downscale if not possible
    if (hpx > win_size_px_h || wpx > win_size_px_w) {
        const int new_h = std::min(h, int(double(w) / ratio));
        const int new_w = std::min(w, int(double(h) * ratio));
        spdlog::debug("[{}] Rescaling image [{}x{}] to [{}x{}], screen: [{}x{}], window: [{}x{}]", id_, hpx, wpx, new_h,
                      new_w, screen_px_h, screen_px_w, h, w);
        command.add('c', new_w, 'r', new_h);
    }
}

void Image::clear(int id) {
    Command command{nvim_, 'a', 'd', 'd', 'i', 'i', id_, 'q', 2};
    if (id) {
        spdlog::debug("[{}] Clearing image with id {}", id_, id * 10000 + id_);
        command.add('p', id * 10000 + id_);
    }
}

} // namespace kitty
