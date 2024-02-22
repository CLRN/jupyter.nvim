
module;
#include <msgpack.hpp>

export module rpc;

import std;
import asio;

namespace rpc {

using asio::experimental::coro;
using asio::experimental::use_coro;
using asio::ip::tcp;

enum { REQUEST = 0, RESPONSE = 1, NOTIFY = 2 };

export class Socket {
    const std::string host_;
    const std::uint16_t port_;
    asio::any_io_executor executor_;
    tcp::socket socket_;

public:
    Socket(asio::any_io_executor& ex, std::string host, std::uint16_t port)
        : host_{std::move(host)}
        , port_(port)
        , executor_(ex)
        , socket_{ex} {}

    auto connect() -> asio::awaitable<void> {
        auto resolver = tcp::resolver{executor_};
        const auto results = co_await resolver.async_resolve(host_, std::to_string(port_), asio::use_awaitable);
        co_await asio::async_connect(socket_, results, asio::use_awaitable);
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

        co_await socket_.async_send(asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);
    }

    //
    // asio::experimental::coro<void, std::size_t> line_logger(asio::any_io_executor ex) {
    //     std::size_t lines_read = 0u;
    //     auto reader = receive(ex);
    //     // auto reader = socket.receive();
    //     while (auto l = co_await reader) {
    //         std::cout << "Read: '" << *l << "'" << std::endl;
    //         lines_read++;
    //     }
    //     co_return lines_read;
    coro<std::string_view> line_reader(tcp::socket stream) {
        while (stream.is_open()) {
            std::array<char, 4096> buf;

            auto read = co_await stream.async_read_some(asio::buffer(buf), use_coro);

            if (read == 0u)
                continue;

            co_yield std::string_view{buf.data(), read};
        }
    }

    coro<void, std::size_t> line_logger(tcp::socket stream) {
        std::size_t lines_read = 0u;
        auto reader = line_reader(std::move(stream));
        while (auto l = co_await reader) {
            std::cout << "Read: '" << *l << "'" << std::endl;
            lines_read++;
        }
        co_return lines_read;
    }

    void read_lines(tcp::socket sock) {
        co_spawn(line_logger(std::move(sock)), [](std::exception_ptr, std::size_t lines) {
            std::clog << "Read " << lines << " lines" << std::endl;
        });
    }
};

auto run_io(asio::io_context& ctx, const std::string& host, uint16_t port) -> asio::awaitable<void> {
    auto ex = co_await asio::this_coro::executor;
    auto resolver = tcp::resolver{ex};
    const auto results = co_await resolver.async_resolve(host, std::to_string(port), asio::use_awaitable);

    tcp::socket socket{ctx};
    co_await asio::async_connect(socket, results, asio::use_awaitable);

    auto receive = [](tcp::socket& socket) -> coro<msgpack::object> {
        msgpack::unpacker pac;
        pac.reserve_buffer(1024);
        while (true) {
            const auto n = co_await socket.async_read_some(asio::buffer(pac.buffer(), pac.buffer_capacity()), use_coro);
            pac.buffer_consumed(n);
            msgpack::object_handle oh;
            while (pac.next(oh)) {
                co_yield oh.get();
            }
        }
    };

    while (auto l = co_await receive(socket)) {
        std::cout << "Read: '" << *l << "'" << std::endl;
        lines_read++;
    }
}

export auto run(const std::string& host, uint16_t port) {
    auto ctx = asio::io_context{};

    // auto socket = Socket{};

    // auto runner = [&] -> asio::awaitable<void> {
    //     co_await socket.connect();
    //     auto server = [&] -> asio::awaitable<void> {
    //         while (true) {
    //             co_await socket.send("whatever", 1, 2, "test");
    //             auto timer = asio::steady_timer{co_await asio::this_coro::executor, std::chrono::seconds(5)};
    //             co_await timer.async_wait(asio::use_awaitable);
    //         }
    //     };
    //
    //     auto client = [&](asio::any_io_executor ex) -> asio::experimental::coro<void> {
    //         // auto reader = socket.receive(ex);
    //         // std::size_t lines_read = 0u;
    //         // while (auto l = co_await reader) {
    //         //     std::cout << "Read: '" << *l << "'" << std::endl;
    //         //     lines_read++;
    //         // }
    //         // auto msg = co_await reader.async_resume(asio::use_awaitable);
    //         // std::cout << "Read: '" << *msg << "'" << std::endl;
    //         // return lines_read;
    //     };
    //
    //     asio::co_spawn(ctx, server, asio::detached);
    //     // co_await client(co_await asio::this_coro::executor).async_resume(asio::use_awaitable);
    //     // asio::co_spawn(client, asio::detached);
    //     // asio::co_spawn(client([](std::exception_ptr, std::size_t lines) {
    //     //     std::clog << "Read " << lines << " lines" << std::endl;
    //     // }));
    // };

    asio::co_spawn(ctx, run_io(ctx, host, port), asio::detached);
    ctx.run();
}
} // namespace rpc
// auto serve_client(tcp::socket socket) -> asio::awaitable<void> {
//     using namespace std::chrono;
//
//     std::cout << "New client connected\n";
//
//     auto ex = co_await asio::this_coro::executor;
//     auto timer = asio::system_timer{ex};
//     auto counter = 0;
//     while (true) {
//         try {
//             auto s = std::to_string(counter);
//             auto buf = asio::buffer(s.data(), s.size());
//             auto n = co_await async_write(socket, buf, asio::use_awaitable);
//             std::cout << "Wrote " << n << " byte(s)\n";
//             ++counter;
//             timer.expires_after(100ms);
//             co_await timer.async_wait(asio::use_awaitable);
//         } catch (...) { // Error or client disconnected
//             break;
//         }
//     }
// }
//
// auto listen(tcp::endpoint endpoint) -> asio::awaitable<void> {
//     auto ex = co_await asio::this_coro::executor;
//     auto a = tcp::acceptor{ex, endpoint};
//     while (true) {
//         auto socket = co_await a.async_accept(asio::use_awaitable);
//         auto session = [s = std::move(socket)]() mutable {
//             auto awaitable = serve_client(std::move(s));
//             return awaitable;
//         };
//         asio::co_spawn(ex, std::move(session), asio::detached);
//     }
// }
//
// int tmain() {
//     auto server = [] {
//         auto endpoint = tcp::endpoint{tcp::v4(), 37259};
//         auto awaitable = listen(endpoint);
//         return awaitable;
//     };
//     auto ctx = asio::io_context{};
//     asio::co_spawn(ctx, server, asio::detached);
//     ctx.run(); // Run event loop from main thread
//     return 0;
// }
//
// auto connect(const std::string &host, std::uint8_t port)
//     -> asio::awaitable<void> {
//     return {};
// }
