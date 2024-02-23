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

#include <iostream>
#include <msgpack.hpp>

namespace rpc {

using boost::asio::ip::tcp;
using namespace boost::asio::experimental::awaitable_operators;

enum { REQUEST = 0, RESPONSE = 1, NOTIFY = 2 };

class Socket {
    const std::string host_;
    const std::uint16_t port_;
    std::optional<tcp::socket> socket_;
    std::uint32_t msgid_ = 0;

public:
    Socket(std::string host, std::uint16_t port)
        : host_{std::move(host)}
        , port_(port) {}

    auto connect() -> boost::asio::awaitable<void> {
        auto ex = co_await boost::asio::this_coro::executor;
        auto resolver = tcp::resolver{ex};
        const auto results = co_await resolver.async_resolve(host_, std::to_string(port_), boost::asio::use_awaitable);

        auto socket = tcp::socket{ex};
        co_await boost::asio::async_connect(socket, results, boost::asio::use_awaitable);
        socket_ = std::move(socket);
    }

    template <typename... U>
    auto send(const std::string& method, const U&... u) -> boost::asio::awaitable<void> {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack_array(4);
        pk.pack(static_cast<std::uint32_t>(REQUEST));
        pk.pack(msgid_++);
        pk.pack(method);
        pk.pack_array(sizeof...(u));
        int _[] = {(pk.pack(u), 0)...};

        co_await socket_->async_send(boost::asio::buffer(buffer.data(), buffer.size()), boost::asio::use_awaitable);
    }

    auto receive(boost::asio::io_context&) -> boost::asio::experimental::coro<msgpack::object> {
        msgpack::unpacker pac;
        pac.reserve_buffer(1024);

        while (true) {
            const auto n = co_await socket_->async_read_some(boost::asio::buffer(pac.buffer(), pac.buffer_capacity()),
                                                             boost::asio::experimental::use_coro);
            pac.buffer_consumed(n);

            msgpack::object_handle oh;
            while (pac.next(oh)) {
                co_yield oh.get();
            }
        }
    }
};

// Read: '[1,3,[0,"Wrong number of arguments: expecting 1 but got 0"],null]'
// Read: '[1,6,null,"#"]'
// Read: '[1,7,null,null]' - set line success
//
inline auto run(std::string host, uint16_t port) {
    auto ctx = boost::asio::io_context{};
    auto socket = Socket{std::move(host), port};

    auto runner = [&] -> boost::asio::awaitable<void> {
        co_await socket.connect();

        auto server = [&]() -> boost::asio::awaitable<void> {
            while (true) {
                // co_await socket.send("nvim_get_current_line");
                // co_await socket.send("nvim_set_current_line", "test");
                // co_await socket.send("nvim_win_get_cursor", 0);
                co_await socket.send("nvim_win_get_cursor", 0);
                auto timer =
                    boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::seconds(5)};
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            std::cout << "returned from server" << std::endl;
        };

        auto client = [&](boost::asio::io_context& ctx) -> boost::asio::experimental::coro<void> {
            auto reader = socket.receive(ctx);
            while (auto l = co_await reader) {
                std::cout << "Read: '" << *l << "'" << std::endl;
            }
        };

        auto coro = client(ctx);
        boost::asio::co_spawn(ctx, server, boost::asio::detached);
        co_await coro.async_resume(boost::asio::use_awaitable);
        // co_await (server() && coro.async_resume(boost::asio::use_awaitable));
    };

    boost::asio::co_spawn(ctx, runner, boost::asio::detached);

    try {
        const int cnt = ctx.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
}
} // namespace rpc
