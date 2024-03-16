#pragma once

#include "api.hpp"
#include "executor.hpp"
#include "graphics.hpp"
#include "printer.hpp"
#include "window.hpp"

#include <boost/cobalt/promise.hpp>
#include <spdlog/spdlog.h>

namespace nvim {

template <typename Backend>
class Image {
    Graphics& graphics_;
    Backend image_;
    int mark_id_{};
    const std::string path_;
    const std::string buffer_path_;
    int buf_line_{};
    int screen_line_{};
    bool visible_{};

    auto read_output(boost::process::async_pipe pipe) -> boost::cobalt::promise<std::vector<std::uint8_t>>;
    auto place_image(const nvim::Window& win) -> void;
    auto update_line() -> boost::cobalt::promise<void>;

public:
    Image(Graphics& graphics, std::string buffer_path, std::string path, int line) // initial image position
        : graphics_{graphics}
        , image_{graphics_}
        , path_{std::move(path)}
        , buffer_path_{std::move(buffer_path)}
        , buf_line_{line} {}

    auto load() -> boost::cobalt::promise<void>;
    auto place(int virt_offset, int buf, int win_id) -> boost::cobalt::promise<int>;
    auto clear(int win_id) -> boost::cobalt::promise<void>;
};

template <typename Backend>
auto Image<Backend>::load() -> boost::cobalt::promise<void> {
    if (path_.starts_with("http")) {
        boost::process::async_pipe ap{nvim::ExecutorSingleton::context()};

        spdlog::info("Fetching image {}", path_);

        try {
            boost::process::child process(boost::process::search_path("curl"), path_, "-o", "-", "-s",
                                          boost::process::std_out > ap);
            const auto data = co_await read_output(std::move(ap));

            spdlog::info("Got {} bytes for {}", data.size(), path_);

            image_.load(data);
        } catch (const std::exception& e) {
            spdlog::error("Failed to load image from {}, error: {}", path_, e.what());
        }
    } else {
        image_.load((boost::filesystem::path(buffer_path_).parent_path() / path_).string());
    }
}

template <typename Backend>
auto Image<Backend>::place_image(const nvim::Window& win) -> void {

    if (visible_) {
        image_.place(nvim::Point{.x = 0, .y = static_cast<int>(screen_line_)}, win);
    } else {
        image_.clear(win);
    }
}

template <typename Backend>
auto Image<Backend>::update_line() -> boost::cobalt::promise<void> {
    static const int ns_id = co_await graphics_.api().nvim_create_namespace("jupyter");
    if (mark_id_) {
        const auto mark = co_await graphics_.api().nvim_buf_get_extmark_by_id(0, ns_id, mark_id_, {});
        const auto& val = mark.as_vector();
        if (!val.empty()) {
            buf_line_ = val.at(0).as_uint64_t();
        }
    }
}

template <typename Backend>
auto Image<Backend>::place(int virt_offset, int buf, int win_id) -> boost::cobalt::promise<int> {
    static const int ns_id = co_await graphics_.api().nvim_create_namespace("jupyter");

    const auto [window, _] = co_await boost::cobalt::join(nvim::Window::get(graphics_, win_id), update_line());
    const auto [vis_from, vis_to] = window.visibility();
    const auto area = image_.area(window);
    const auto screen_line = buf_line_ + virt_offset + 1 - vis_from;
    const auto is_visible = screen_line - 1 >= 0 && screen_line - 1 + area.h <= window.size().h;
    const auto redraw = (screen_line != screen_line_ && (visible_ || is_visible)) || visible_ != is_visible;

    screen_line_ = screen_line;
    visible_ = is_visible;

    if (!redraw) {
        co_return visible_ ? area.h : 0;
    }

    place_image(window);

    if (!mark_id_) {
        // fill area with virtual text
        using any = nvim::Api::any;
        std::vector<any> virt_lines(area.h, std::vector<any>{{std::vector<any>{{"", "Comment"}}}});

        mark_id_ = co_await graphics_.api().nvim_buf_set_extmark(buf, ns_id, buf_line_, 0,
                                                                 {{"virt_lines", std::move(virt_lines)}});

        spdlog::info("Aligning image at line {} size {} with mark {}, window: {}", buf_line_, area, mark_id_, win_id);
    } else if (mark_id_ && !visible_) {
        spdlog::info("Hiding image at line {} size {} with mark {}, window: {}", buf_line_, area, mark_id_, win_id);
        co_await graphics_.api().nvim_buf_del_extmark(buf, ns_id, mark_id_);
        mark_id_ = 0;
    }

    // TODO:
    // 1. editing should update image positions - done
    // 2. scrolling past the image - done
    // 3. working with other windows(preview in telescope)

    co_return visible_ ? area.h : 0;
}

template <typename Backend>
auto Image<Backend>::clear(int win_id) -> boost::cobalt::promise<void> {
    const auto win = co_await nvim::Window::get(graphics_, win_id);
    image_.clear(win);
}

template <typename Backend>
auto Image<Backend>::read_output(boost::process::async_pipe pipe) -> boost::cobalt::promise<std::vector<std::uint8_t>> {
    std::vector<std::uint8_t> buf(4096);
    boost::system::error_code e{};
    std::size_t n{};
    std::size_t total{};

    while (!e) {
        buf.resize(buf.size() * 2);
        std::tie(e, n) =
            co_await boost::asio::async_read(pipe, boost::asio::buffer(buf.data() + total, buf.size() - total),
                                             boost::asio::as_tuple(boost::cobalt::use_op));
        total += n;
    }

    buf.resize(total);
    co_return buf;
}
} // namespace nvim
