#include "boost/cobalt/race.hpp"
#include "fmt/format.h"
#include "kitty.hpp"
#include "nvim.hpp"
#include "nvim_graphics.hpp"
// #include "spdlog/cfg/env.h"
// #include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"
#include <boost/process.hpp>
#include <cassert>
#include <chrono>
#include <fstream>
#include <ios>
#include <iterator>
#include <ostream>
#include <string>

#include <boost/beast/core/detail/base64.hpp>

auto print_map(int win, const auto& data) {

    std::cout << "win: " << win << ", ";

    for (const auto& [k, v] : data) {
        std::cout << k.as_string() << "=" << (v.is_string() ? v.as_string() : std::to_string(v.as_uint64_t())) << ", ";
    }
    std::cout << std::endl;
}

auto run() -> boost::cobalt::task<int> {
    // // ::setenv("TERM_PROGRAM", "kitty", 0);
    // std::ifstream ifs{"/code/jupyter.nvim/data", std::ios::binary};
    //
    // assert(ifs.is_open());
    // std::string buf;
    // std::copy(std::istream_iterator<char>(ifs), std::istream_iterator<char>(), std::back_inserter(buf));
    // std::cerr << buf << std::endl;
    // // {
    // //     std::ofstream ofs("/dev/pts/0", std::ios::binary);
    // //     ofs.write(buf.data(), buf.size());
    // // }
    // co_return 0;
    //
    // spdlog::cfg::load_env_levels();
    //
    // spdlog::flush_on(spdlog::level::debug);
    // spdlog::set_level(spdlog::level::debug);
    //
    // // const auto sink = std::make_shared<spdlog::sinks::stdout_sink_st>();
    // const auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/jupyter.cpp.log");
    // const auto terminal_logger = std::make_shared<spdlog::logger>("terminal", sink);
    // spdlog::initialize_logger(terminal_logger);
    //
    // const auto flags = Flags::instance();
    // flags->use_escape_codes = true;
    // Terminal t{};
    // terminal_logger->debug("ioctl sizes: COLS={} ROWS={} XPIXEL={} YPIXEL={}", t.cols, t.rows, t.font_width,
    // t.font_height); co_return 0;

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

        auto draw(nvim::Api& api, int win_id) -> boost::cobalt::promise<void> {
            if (!windows_.insert(win_id).second)
                co_return;

            const auto pos = co_await api.nvim_win_get_position(win_id);
            const auto w = co_await api.nvim_win_get_width(win_id);
            const auto h = co_await api.nvim_win_get_height(win_id);
            std::cout << pos[0] << " " << pos[1] << " w " << w << " h " << h << std::endl;

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
                    // api.nvim_notify("loading image...", 2, {}),
                    co_await boost::cobalt::join(api.nvim_buf_set_lines(id, 0, -1, false, {""}),
                                                 api.nvim_buf_set_option(id, "buftype", "nowrite"));
                    Buffer b{remote, static_cast<int>(id), data.find("file")->second.as_string()};
                    it = buffers.emplace(id, std::move(b)).first;
                }

                const auto win = co_await api.nvim_get_current_win();
                print_map(win, data);
                co_await it->second.draw(api, win);
            } else if (event == "BufLeave") {
                const auto it = buffers.find(id);
                if (it != buffers.end()) {
                    const auto win = co_await api.nvim_get_current_win();
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
                const auto win = co_await api.nvim_get_current_win();
                print_map(win, data);
                co_await it->second.draw(api, win);
            } else if (event == "WinClosed") {
                const auto win = std::stoi(data.find("file")->second.as_string());
                it->second.clear(win);
            }
        }
    };

    co_await boost::cobalt::join(handle_buffers(), handle_windows());

    // auto window_events = api.nvim_create_autocmd(
    //     {
    //         "WinLeave",
    //         "WinEnter",
    //         "WinResized",
    //         "WinScrolled",
    //     },
    //     {
    //         {"group", augroup},
    //     });
    //
    // while (buffer_events && window_events) {
    //     const auto res = co_await boost::cobalt::race(buffer_events, window_events);
    //     const auto& msg = res.index() ? boost::variant2::get<1>(res) : boost::variant2::get<0>(res);
    //
    //     const auto data = msg.as_vector().front().as_multimap();
    //     const auto event = data.find("event")->second.as_string();
    //
    //     const auto win = co_await api.nvim_get_current_win();
    //     std::cout << "win: " << win << ", ";
    //
    //     for (const auto& [k, v] : data) {
    //         std::cout << k.as_string() << "=" << (v.is_string() ? v.as_string() : std::to_string(v.as_uint64_t()))
    //                   << ", ";
    //     }
    //     std::cout << std::endl;
    //
    //     if (event.ends_with("Enter")) {
    //         const auto pos = co_await api.nvim_win_get_position(win);
    //         std::cout << pos[0] << " " << pos[1] << std::endl;
    //         im.place(pos.back(), pos.front());
    //     } else if (event.ends_with("Leave")) {
    //         im.clear();
    //     }
    // }
    //
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // for (int i = 0; i < 50; ++i) {
    //     im.place(i * 2, 10);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }

    // const auto size = co_await graphics.screen_size();
    // std::cout << size.first << " " << size.second << std::endl;

    co_return 0;

    // const auto screen_size = co_await api.nvim_exec2("source /code/jupyter.nvim/test.lua", {{"output", true}});
    // const auto size = screen_size.find("output")->second.as_string();

    // Terminal t{pid};

    std::cout << co_await api.nvim_get_current_line() << std::endl;

    //
    //
    //     co_await client.call("nvim_exec2", func, std::map<std::string, std::string>{});
    //     co_await client.call("nvim_call_function", fmt::format("HandleBuffer{0}", client.channel()),
    //                          std::vector<std::string>{});
    //
    //     std::cout << "notifications: " << (co_await client.notifications()).as_string() << std::endl;
    //     std::cout << (co_await client.call("nvim_eval", "(3 + 2) * 4")).as_uint64_t() << std::endl;
    //     std::cout << (co_await client.call("nvim_eval", "(3 + 2) * 4.1")).as_double() << std::endl;
    //
    //     std::cout << "get_current_line = " << (co_await client.call("nvim_get_current_line")).as_string() <<
    //     std::endl;
    //
    //     co_await client.call("nvim_set_current_line", "hello");
    //
    //     std::cout << "get_current_line = " << (co_await client.call("nvim_get_current_line")).as_string() <<
    //     std::endl;
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
