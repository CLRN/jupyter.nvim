export module as;

import std;
import asio;
import msgpack;

using asio::ip::tcp;

export class Socket {
    const std::string host_;
    const std::uint8_t port_;
    std::optional<tcp::socket> socket_;

public:
    Socket(std::string host, std::uint8_t port)
        : host_{std::move(host)}, port_(port) {}

    auto connect() -> asio::awaitable<void> {
        auto ex = co_await asio::this_coro::executor;
        auto resolver = tcp::resolver{ex};
        const auto results = co_await resolver.async_resolve(
            host_, std::to_string(port_), asio::use_awaitable);

        auto socket = tcp::socket{ex};
        co_await asio::async_connect(socket, results, asio::use_awaitable);
        socket_ = std::move(socket);
    }

    auto write(const std::vector<char> &buffer) -> asio::awaitable<void> {
        asio::error_code ec;
        auto n = co_await socket_->async_send(
            asio::buffer(buffer),
            asio::redirect_error(asio::use_awaitable, ec));
    }

    auto read() -> asio::awaitable<std::vector<char>> {
        char buffer[4096];
        asio::error_code ec;
        auto n = co_await socket_->async_read_some(
            asio::buffer(buffer),
            asio::redirect_error(asio::use_awaitable, ec));

        co_return std::vector<char>(buffer, buffer + n);
    }
};

auto msgPackTest() { 
    pack::pack_request("test", 1, "ss");
}

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
