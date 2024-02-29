#include "nvim.hpp"
#include <fmt/core.h>

auto run() -> boost::cobalt::task<int> {
    auto api = co_await nvim::Api::create("localhost", 6666);
    std::cout << co_await api.nvim_get_current_line() << std::endl;

    auto generator = api.nvim_create_autocmd("BufEnter", {});
    while (generator) {
        auto msg = co_await generator;
        const auto buf = msg.as_vector().front().as_uint64_t();
        std::cout << "new buf " << buf << std::endl;
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
    boost::asio::io_context ctx{BOOST_ASIO_CONCURRENCY_HINT_1};
    boost::cobalt::this_thread::set_executor(ctx.get_executor());
    auto f = boost::cobalt::spawn(ctx, run(), boost::asio::use_future);
    ctx.run();
}
