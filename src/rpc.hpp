#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/cobalt.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/process.hpp>

#include <cstdint>
#include <iostream>
#include <msgpack.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace rpc {

using boost::asio::ip::tcp;
using namespace boost::asio::experimental::awaitable_operators;

enum { REQUEST = 0, RESPONSE = 1, NOTIFY = 2 };

class Socket {
    const std::string host_;
    const std::uint16_t port_;
    std::optional<tcp::socket> socket_;

public:
    Socket(std::string host, std::uint16_t port)
        : host_{std::move(host)}
        , port_(port) {}

    auto connect() -> boost::cobalt::task<void> {
        auto ex = co_await boost::asio::this_coro::executor;
        auto resolver = tcp::resolver{ex};
        const auto results = co_await resolver.async_resolve(host_, std::to_string(port_), boost::cobalt::use_op);

        auto socket = tcp::socket{ex};
        co_await boost::asio::async_connect(socket, results, boost::cobalt::use_op);
        socket_ = std::move(socket);
    }

    template <typename... U>
    auto send(std::uint32_t msgid, const std::string& method, const U&... u) -> boost::cobalt::promise<void> {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);

        pk.pack_array(4);
        pk.pack(static_cast<std::uint32_t>(REQUEST));
        pk.pack(msgid++);
        pk.pack(method);
        pk.pack_array(sizeof...(u));
        int _[] = {(pk.pack(u), 0)...};

        co_await socket_->async_send(boost::asio::buffer(buffer.data(), buffer.size()), boost::cobalt::use_op);
    }

    auto receive(boost::asio::io_context&) -> boost::cobalt::generator<msgpack::object> {
        msgpack::unpacker pac;
        const auto increase = pac.buffer_capacity();

        while (true) {
            pac.reserve_buffer(increase);
            const auto n = co_await socket_->async_read_some(boost::asio::buffer(pac.buffer(), pac.buffer_capacity()),
                                                             boost::cobalt::use_op);
            pac.buffer_consumed(n);

            msgpack::object_handle oh;
            while (pac.next(oh)) {
                co_yield oh.get();
            }
        }
    }
};

class Client {
    std::uint32_t msgid_ = 0;
    Socket socket_;
    std::unordered_map<std::uint32_t, std::optional<boost::cobalt::channel<msgpack::object>>> requests_;
    boost::cobalt::task<void> init_task_;

private:
    auto receive(boost::asio::io_context& ctx) -> boost::cobalt::task<void> {
        co_await init_task_;
        auto reader = socket_.receive(ctx);
        while (true) {
            auto obj = co_await reader;
            std::cout << obj << std::endl;
        }
    }

public:
    Client(Socket socket, boost::asio::io_context& ctx)
        : socket_(std::move(socket))
        , init_task_{socket_.connect()} {
        boost::cobalt::spawn(ctx, receive(ctx), boost::asio::detached);
    }

    template <typename... U>
    auto send(const std::string& method, const U&... u) -> boost::cobalt::promise<msgpack::object> {
        co_await init_task_;
        auto& channel = requests_[msgid_++].emplace(1);
        co_return co_await channel.read();
    }
};

// Read: '[1,3,[0,"Wrong number of arguments: expecting 1 but got 0"],null]'
// Read: '[1,6,null,"#"]'
// Read: '[1,7,null,null]' - set line success
//
inline auto run(std::string host, uint16_t port) {
    auto ctx = boost::asio::io_context{};
    auto socket = Socket{std::move(host), port};
    auto client = Client{std::move(socket), ctx};

    // boost::process::ipstream out;
    // std::future<std::string> data;
    // if (boost::process::system("nvim --api-info", boost::process::std_out > data)) {
    //     throw std::runtime_error("unable to read nvim API info");
    // }
    // const auto s = data.get();
    // const auto obj = msgpack::unpack(s.data(), s.size());
    // std::cout << obj.get() << std::endl;

    auto runner = [&] -> boost::cobalt::task<void> {
        co_await client.send("nvim_get_all_options_info");
        // co_await socket.connect();
        //
        // auto server = [&]() -> boost::asio::awaitable<void> {
        //     while (true) {
        //         // co_await socket.send("nvim_get_current_line");
        //         // co_await socket.send("nvim_set_current_line", "test");
        //         // co_await socket.send("nvim_win_get_cursor", 0);
        //         // co_await socket.send("nvim_win_get_cursor", 0);
        //         // function vim.api.nvim_buf_set_lines(buffer, start, end_, strict_indexing, replacement) end
        //         // co_await socket.send("nvim_buf_set_lines", 0, 0, 20, false, std::vector<std::string>{"aa", "bb"});
        //         // next to try:
        //         // co_await socket.send("nvim_buf_set_extmark", 0, 0, 20, 5,
        //         //                      std::vector<std::string>{std::vector<std::string>{"test"}, "O"});
        //
        //         // local mark_id = vim.api.nvim_buf_set_extmark(vim.api.nvim_get_current_buf(), ns_id, line_num,
        //         // col_num, {
        //         //   virt_lines = virt_lines,
        //         //   sign_text = "O",
        //         // })
        //         // co_await socket.send("nvim_get_all_options_info");
        //         auto timer =
        //             boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::seconds(5)};
        //         co_await timer.async_wait(boost::asio::use_awaitable);
        //     }
        // };
        //
        // auto client = [&](boost::asio::io_context& ctx) -> boost::asio::experimental::coro<void> {
        //     auto reader = socket.receive(ctx);
        //     while (auto l = co_await reader) {
        //         std::cout << *l << std::endl;
        //     }
        // };
        //
        // auto coro = client(ctx);
        // boost::asio::co_spawn(ctx, server, boost::asio::detached);
        // co_await coro.async_resume(boost::asio::use_awaitable);
        // co_await (server() && coro.async_resume(boost::asio::use_awaitable));
    };

    try {
        boost::cobalt::spawn(ctx, runner(), boost::asio::detached);
        const int cnt = ctx.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
}
} // namespace rpc
