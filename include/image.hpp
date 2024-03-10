#pragma once

#include "api.hpp"
#include "executor.hpp"
#include "graphics.hpp"
#include "printer.hpp"
#include "window.hpp"

#include <cstddef>

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
    std::size_t line_{};

    auto read_output(boost::process::async_pipe pipe) -> boost::cobalt::promise<std::vector<std::uint8_t>>;

public:
    Image(Graphics& graphics, std::string buffer_path, std::string path, size_t line)
        : graphics_{graphics}
        , image_{graphics_}
        , path_{std::move(path)}
        , buffer_path_{std::move(buffer_path)}
        , line_{line} {}

    auto load() -> boost::cobalt::promise<void>;
    auto place(int y_offset, int buf, int win_id) -> boost::cobalt::promise<int>;
    auto clear(int id = 0) -> void;
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
auto Image<Backend>::place(int y_offset, int buf, int win_id) -> boost::cobalt::promise<int> {
    static const int ns_id = co_await graphics_.api().nvim_create_namespace("jupyter");

    if (mark_id_) {
        const auto mark = co_await graphics_.api().nvim_buf_get_extmark_by_id(0, ns_id, mark_id_, {{"details", true}});
        spdlog::info("existing mark: {}", mark);
    }

    const auto area = image_.place(nvim::Point{.x = 0, .y = static_cast<int>(y_offset + line_ + 1)},
                                   co_await nvim::Window::get(graphics_, win_id));
    // fill area with virtual text
    using any = nvim::Api::any;
    std::vector<any> virt_lines(area.h, std::vector<any>{{std::vector<any>{{"", "Comment"}}}});

    mark_id_ =
        co_await graphics_.api().nvim_buf_set_extmark(buf, ns_id, line_, 1, {{"virt_lines", std::move(virt_lines)}});

    spdlog::info("Aligning image at line {} size {} with mark {}, window: {}", line_, area, mark_id_, win_id);
    co_return area.h;
}

template <typename Backend>
auto Image<Backend>::clear(int id) -> void {
    image_.clear(id);
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
