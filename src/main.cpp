#include "fmt/format.h"
#include "kitty.hpp"
#include "nvim.hpp"
#include "nvim_graphics.hpp"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"
#include "terminal.hpp"
#include <boost/process.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <ostream>
#include <string>

#include <boost/beast/core/detail/base64.hpp>

std::string get_data(const std::string& name) {
    std::ifstream ifs{name, std::ios::binary};
    ifs >> std::noskipws;

    assert(ifs.is_open());
    std::string buf;
    std::copy(std::istream_iterator<char>(ifs), std::istream_iterator<char>(), std::back_inserter(buf));
    return buf;
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
    // const auto size = co_await graphics.screen_size();
    //
    // std::cout << size.first << " " << size.second << std::endl;

    auto remote = co_await graphics.remote();
    kitty::Image im{remote, get_data("/code/jupyter.nvim/1064")};
    for (int i = 0; i < 50; ++i) {
        im.place(i * 2, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    co_return 0;

    // const auto screen_size = co_await api.nvim_exec2("source /code/jupyter.nvim/test.lua", {{"output", true}});
    // const auto size = screen_size.find("output")->second.as_string();

    // Terminal t{pid};

    std::cout << co_await api.nvim_get_current_line() << std::endl;

    auto generator = api.nvim_create_autocmd({"BufWritePost", "BufReadPost"},
                                             {{"pattern", std::vector<nvim::Api::any>{"*.lua", "*.toml"}}});
    while (generator) {
        auto msg = co_await generator;
        const auto buf = msg.as_vector().front().as_multimap();
        std::cout << "read/write" << std::endl;
        for (const auto& [k, v] : buf) {
            std::cout << k.as_string() << "=" << (v.is_string() ? v.as_string() : std::to_string(v.as_uint64_t()))
                      << std::endl;
        }
    }
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
