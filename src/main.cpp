#include "boost/cobalt/join.hpp"
#include "kitty.hpp"
#include "nvim.hpp"
#include "nvim_graphics.hpp"
#include "printer.hpp"

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include <boost/process.hpp>
#include <cassert>
#include <string>

auto run() -> boost::cobalt::task<int> {
    spdlog::set_level(spdlog::level::debug);
    spdlog::cfg::load_env_levels();
    spdlog::debug("starting");

    // const auto output = co_await api.nvim_exec2("lua print(vim.fn['getpid']())", {{"output", true}});
    // int pid = std::atoi(output.find("output")->second.as_string().c_str());

    auto api = co_await nvim::Api::create("localhost", 6666);
    auto graphics = nvim::Graphics{api};
    auto size = co_await graphics.screen_size();

    auto remote = co_await graphics.remote();

    const auto augroup = co_await api.nvim_create_augroup("jupyter", {});

    class Buffer {
        const int id_{};
        std::vector<kitty::Image> images_;
        std::set<int> windows_;

    public:
        Buffer(nvim::RemoteGraphics& remote, int id, const std::string& path)
            : id_{id}
            , images_{} {
            images_.emplace_back(kitty::Image{remote, path});
        }

        auto draw(nvim::Api& api) -> boost::cobalt::promise<void> {
            const auto win_id = co_await api.nvim_get_current_win();
            if (!windows_.insert(win_id).second)
                co_return;

            spdlog::debug("Drawing buffer {} on window {}, images {}", id_, win_id, images_.size());

            const auto [pos, w, h] = co_await boost::cobalt::join(
                api.nvim_win_get_position(win_id), api.nvim_win_get_width(win_id), api.nvim_win_get_height(win_id));

            for (auto& im : images_) {
                im.place(pos.back(), pos.front(), w, h, win_id);
            }
        }

        auto clear(int win_id) {
            if (windows_.erase(win_id)) {
                for (auto& im : images_) {
                    im.clear(win_id);
                }
            }
        }
    };

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

            if (event == "BufEnter") {
                auto it = buffers.find(id);
                if (it == buffers.end()) {
                    spdlog::debug("New Buffer event {}", msg);
                    // api.nvim_notify("loading image...", 2, {}),
                    co_await boost::cobalt::join(api.nvim_buf_set_lines(id, 0, -1, false, {""}),
                                                 api.nvim_buf_set_option(id, "buftype", "nowrite"));
                    Buffer b{remote, static_cast<int>(id), data.find("file")->second.as_string()};
                    it = buffers.emplace(id, std::move(b)).first;
                }

                co_await it->second.draw(api);
            } else if (event == "BufLeave") {
                const auto it = buffers.find(id);
                if (it != buffers.end()) {
                    const auto win = co_await api.nvim_get_current_win();
                    spdlog::debug("Left buffer, window {}, data {}", win, msg);
                    it->second.clear(win);
                }
            } else {
                buffers.erase(id);
            }
        }
    };

    const auto handle_windows = [&]() -> boost::cobalt::promise<void> {
        auto gen = api.nvim_create_autocmd(
            {
                "WinClosed", "WinEnter",
                // "WinResized",
                // "WinScrolled",
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
                spdlog::debug("Entered window {}", msg);
                co_await it->second.draw(api);
            } else if (event == "WinClosed") {
                const auto win = std::stoi(data.find("file")->second.as_string());
                spdlog::debug("Closed window {}, data {}", win, msg);
                it->second.clear(win);
            }
        }
    };

    co_await boost::cobalt::join(handle_buffers(), handle_windows());
    co_return 0;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    boost::asio::io_context ctx{BOOST_ASIO_CONCURRENCY_HINT_1};
    boost::cobalt::this_thread::set_executor(ctx.get_executor());
    auto f = boost::cobalt::spawn(ctx, run(), boost::asio::use_future);
    ctx.run();
}
