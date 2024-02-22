module;
#include <msgpack.hpp>

export module rpc;

import std;
import asio;

namespace rpc {

using asio::ip::tcp;

enum { REQUEST = 0, RESPONSE = 1, NOTIFY = 2 };

export class Socket {
    const std::string host_;
    const std::uint16_t port_;
    std::optional<tcp::socket> socket_;

public:
    Socket(std::string host, std::uint16_t port)
        : host_{std::move(host)}
        , port_(port) {}

    auto connect() -> asio::awaitable<void> {
        auto ex = co_await asio::this_coro::executor;
        auto resolver = tcp::resolver{ex};
        const auto results = co_await resolver.async_resolve(host_, std::to_string(port_), asio::use_awaitable);

        auto socket = tcp::socket{ex};
        co_await asio::async_connect(socket, results, asio::use_awaitable);
        socket_ = std::move(socket);
    }

    template <typename... U>
    auto send(const std::string& method, const U&... u) -> asio::awaitable<void> {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        std::uint32_t msgid_ = 0;
        pk.pack_array(4);
        pk.pack(static_cast<std::uint32_t>(REQUEST));
        pk.pack(msgid_++);
        pk.pack(method);
        pk.pack_array(sizeof...(u));
        int _[] = {(pk.pack(u), 0)...};

        co_await socket_->async_send(asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);
    }

    auto receive(asio::io_context&) -> asio::experimental::coro<msgpack::object> {
        msgpack::unpacker pac;
        pac.reserve_buffer(1024);

        while (true) {
            const auto n = co_await socket_->async_read_some(asio::buffer(pac.buffer(), pac.buffer_capacity()),
                                                             asio::experimental::use_coro);
            pac.buffer_consumed(n);

            msgpack::object_handle oh;
            while (pac.next(oh)) {
                co_yield oh.get();
            }
        }
    }
};

export auto run(std::string host, uint16_t port) {
    auto ctx = asio::io_context{};
    auto socket = Socket{std::move(host), port};

    auto runner = [&] -> asio::awaitable<void> {
        co_await socket.connect();

        auto server = [&]() -> asio::awaitable<void> {
            while (true) {
                co_await socket.send("whatever", 1, 2, "test");
                auto timer = asio::steady_timer{co_await asio::this_coro::executor, std::chrono::seconds(5)};
                co_await timer.async_wait(asio::use_awaitable);
            }
        };

        auto client = [&](asio::io_context& ctx) -> asio::experimental::coro<void> {
            auto reader = socket.receive(ctx);
            while (auto l = co_await reader) {
                std::cout << "Read: '" << *l << "'" << std::endl;
            }
        };

        co_spawn(client(ctx), asio::detached);
        co_await server();
    };

    asio::co_spawn(ctx, runner, asio::detached);
    ctx.run();
}
} // namespace rpc
