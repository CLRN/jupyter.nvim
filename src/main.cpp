#include "nvim.hpp"
#include "nvim_graphics.hpp"
#include "handlers/images.hpp"

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include <boost/process.hpp>
#include <cassert>
#include <string>

auto run() -> boost::cobalt::task<int> {
    spdlog::set_level(spdlog::level::debug);
    spdlog::cfg::load_env_levels();
    spdlog::debug("starting");

    auto api = co_await nvim::Api::create("localhost", 6666);
    const auto output = co_await api.nvim_exec2("lua print(vim.fn['getpid']())", {{"output", true}});
    const int pid = std::atoi(output.find("output")->second.as_string().c_str());

    auto graphics = nvim::Graphics{api};
    auto size = co_await graphics.screen_size();
    auto remote = co_await graphics.remote();

    const auto augroup = co_await api.nvim_create_augroup("jupyter", {});
    co_await jupyter::handle_images(api, remote, augroup);

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
