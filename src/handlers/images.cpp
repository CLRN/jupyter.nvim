#include "handlers/images.hpp"
#include "api.hpp"
#include "geometry.hpp"
#include "graphics.hpp"
#include "kitty.hpp"
#include "printer.hpp"
#include "window.hpp"

#include <boost/cobalt/promise.hpp>
#include <spdlog/spdlog.h>

namespace jupyter {
namespace {

class Buffer {
    const int id_{};
    nvim::Graphics& graphics_;
    kitty::Image image_;
    std::set<int> windows_;

public:
    Buffer(nvim::Graphics& graphics, int id, const std::string& path)
        : id_{id}
        , graphics_{graphics}
        , image_{graphics} {
        image_.load(path);
    }

    auto draw(nvim::Api& api, int win_id) -> boost::cobalt::promise<void> {
        if (!windows_.insert(win_id).second)
            co_return;

        spdlog::debug("Drawing buffer {} on window {}", id_, win_id);
        image_.place(nvim::Point{}, co_await nvim::Window::get(graphics_, win_id));
    }

    auto clear(int win_id) -> boost::cobalt::promise<void> {
        if (windows_.erase(win_id)) {
            image_.clear(co_await nvim::Window::get(graphics_, win_id));
        }
    }
};

} // namespace

auto handle_images(nvim::Api& api, nvim::Graphics& graphics, int augroup) -> boost::cobalt::promise<void> {

    std::map<int, Buffer> buffers;

    const auto handle_buffers = [&]() -> boost::cobalt::promise<void> {
        auto gen = api.nvim_create_autocmd(
            {
                "BufDelete",
                "BufEnter",
                "BufLeave",
            },
            {
                {"pattern", std::vector<nvim::Api::any>{"*.png"}},
                {"group", augroup},
            });

        while (gen) {
            auto msg = co_await gen;
            const auto data = msg.as_vector().front().as_multimap();
            const auto event = data.find("event")->second.as_string();
            const auto id = data.find("buf")->second.as_uint64_t();
            const auto win_id = co_await api.nvim_get_current_win();

            if (event == "BufEnter") {
                auto it = buffers.find(id);
                if (it == buffers.end()) {
                    spdlog::debug("New Buffer {}, window: {}", msg, win_id);

                    const auto window = co_await nvim::Window::get(graphics, win_id);

                    // api.nvim_notify("loading image...", 2, {}),
                    //
                    co_await boost::cobalt::join(api.nvim_buf_set_lines(id, 0, -1, false, {""}),
                                                 api.nvim_buf_set_option(id, "buftype", "nowrite"));
                    Buffer b{graphics, static_cast<int>(id), data.find("file")->second.as_string()};
                    it = buffers.emplace(id, std::move(b)).first;
                }

                co_await it->second.draw(api, win_id);
            } else if (event == "BufLeave") {
                const auto it = buffers.find(id);
                if (it != buffers.end()) {
                    const auto win = co_await api.nvim_get_current_win();
                    spdlog::debug("Left buffer, window {}, data {}", win, msg);
                    co_await it->second.clear(win);
                }
            } else {
                buffers.erase(id);
            }
        }
    };

    const auto handle_windows = [&]() -> boost::cobalt::promise<void> {
        auto gen = api.nvim_create_autocmd(
            {
                "WinClosed",
                "WinEnter",
            },
            {
                {"group", augroup},
            });

        while (gen) {
            auto msg = co_await gen;
            const auto data = msg.as_vector().front().as_multimap();
            const auto event = data.find("event")->second.as_string();
            const auto buf = data.find("buf")->second.as_uint64_t();

            auto it = buffers.find(buf);
            if (it == buffers.end())
                continue;

            if (event == "WinEnter") {
                const auto win_id = co_await api.nvim_get_current_win();
                spdlog::debug("Entered window {}, id: {}", msg, win_id);
                co_await it->second.draw(api, win_id);
            } else if (event == "WinClosed") {
                const auto win = std::stoi(data.find("file")->second.as_string());
                spdlog::debug("Closed window {}, data {}", win, msg);
                it->second.clear(win);
            }
        }
    };

    co_await boost::cobalt::join(handle_buffers(), handle_windows());
}
} // namespace jupyter
