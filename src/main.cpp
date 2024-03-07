#include "executor.hpp"
#include "handlers/images.hpp"
#include "handlers/markdown.hpp"
#include "nvim.hpp"
#include "nvim_graphics.hpp"

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

auto run() -> boost::cobalt::task<int> {
    spdlog::set_level(spdlog::level::debug);
    spdlog::cfg::load_env_levels();
    spdlog::debug("starting");

    auto api = co_await nvim::Api::create("localhost", 6666);
    auto graphics = nvim::Graphics{api};
    co_await graphics.init();

    const auto augroup = co_await api.nvim_create_augroup("jupyter", {});
    co_await boost::cobalt::join(jupyter::handle_images(api, graphics, augroup),
                                 jupyter::handle_markdown(api, graphics, augroup));

    co_return 0;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    auto& ctx = nvim::ExecutorSingleton::context();
    boost::cobalt::this_thread::set_executor(ctx.get_executor());
    try {
        auto f = boost::cobalt::spawn(ctx, run(), boost::asio::use_future);
        ctx.run();
        return f.get();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
        return 1;
    }
}
