#pragma once

#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <boost/process.hpp>
#include <msgpack.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace rpc {

using boost::asio::ip::tcp;

enum { REQUEST = 0, RESPONSE = 1, NOTIFY = 2 };

class Socket {
    const std::string host_;
    const std::uint16_t port_;
    std::optional<tcp::socket> socket_;

public:
    Socket(std::string host, std::uint16_t port)
        : host_{std::move(host)}
        , port_(port) {}

    auto connect() -> boost::cobalt::promise<void> {
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

    auto receive() -> boost::cobalt::generator<msgpack::object> {
        msgpack::unpacker pac;
        const auto increase = pac.buffer_capacity();

        try {
            while (true) {
                pac.reserve_buffer(increase);
                const auto n = co_await socket_->async_read_some(
                    boost::asio::buffer(pac.buffer(), pac.buffer_capacity()), boost::cobalt::use_op);
                pac.buffer_consumed(n);

                msgpack::object_handle oh;
                while (pac.next(oh)) {
                    co_yield oh.get();
                }
            }
        } catch (const boost::system::system_error& e) {
            if (e.code() == boost::system::errc::operation_canceled)
                co_return {};

            std::clog << "receive exception: " << e.what() << std::endl;
        }
        co_return {};
    }
};

class Client {
    using DataType = std::variant<msgpack::type::variant, std::exception_ptr>;

    std::uint32_t msgid_ = 0;
    Socket socket_;
    std::unordered_map<std::uint32_t, std::optional<boost::cobalt::channel<DataType>>> requests_;
    boost::cobalt::channel<msgpack::type::variant> notification_channel_;
    boost::cobalt::promise<void> receive_task_;
    volatile bool connected_{};

private:
    auto receive() -> boost::cobalt::promise<void> {
        using Response = std::tuple<std::uint32_t, std::uint32_t, msgpack::type::variant, msgpack::type::variant>;

        co_await socket_.connect();
        connected_ = true;

        auto reader = socket_.receive();
        while (reader) {
            const auto obj = co_await reader;
            std::cout << obj << std::endl;
            if (obj.is_nil())
                break;

            const auto& [type, id, error, result] = obj.as<Response>();
            if (type == RESPONSE) {
                const auto it = requests_.find(id);
                if (it != requests_.end()) {
                    if (error.is_nil()) {
                        co_await it->second->write(result);
                    } else {
                        // errors are returned as array of two elements, where the message is in the end
                        co_await it->second->write(
                            std::make_exception_ptr(std::runtime_error(error.as_vector().back().as_string())));
                    }
                }
            } else if (type == NOTIFY) {
                co_await notification_channel_.write(result);
            }
        }
    }

public:
    Client(Socket socket)
        : socket_(std::move(socket))
        , receive_task_{receive()} {}

    ~Client() {
        receive_task_.cancel();
        receive_task_.attach();
    }

    template <typename... U>
    auto call(const std::string& method, const U&... u) -> boost::cobalt::promise<msgpack::type::variant> {
        while (!connected_) {
            boost::asio::steady_timer tim{co_await boost::asio::this_coro::executor, std::chrono::milliseconds(100)};
            co_await tim.async_wait(boost::cobalt::use_op);
        }
        const auto id = msgid_++;

        auto& channel = requests_[id].emplace(1);

        co_await socket_.send(id, method, std::forward(u)...);

        auto response = co_await channel.read();
        requests_.erase(id);
        if (const auto exc = std::get_if<std::exception_ptr>(&response)) {
            std::rethrow_exception(*exc);
        } else {
            co_return std::get<msgpack::type::variant>(response);
        }
    }

    auto notifications() -> boost::cobalt::generator<msgpack::type::variant> {
        while (notification_channel_.is_open()) {
            co_yield co_await notification_channel_.read();
        }
        co_return {};
    }
};

// Read: '[1,3,[0,"Wrong number of arguments: expecting 1 but got 0"],null]'
// Read: '[1,6,null,"#"]'
// Read: '[1,7,null,null]' - set line success
//
inline auto run(std::string host, uint16_t port) -> boost::cobalt::promise<void> {
    auto socket = Socket{std::move(host), port};
    auto client = Client{std::move(socket)};
    const auto info = co_await client.call("nvim_get_all_options_info");
    std::cout << info.which() << std::endl;

    {
        const auto error = co_await client.call("nvim_set_current_line");
        std::cout << error.which() << std::endl;
    }
    {
        const auto error = co_await client.call("nvim_win_get_cursor");
        std::cout << error.which() << std::endl;
    }

    // boost::process::ipstream out;
    // std::future<std::string> data;
    // if (boost::process::system("nvim --api-info", boost::process::std_out > data)) {
    //     throw std::runtime_error("unable to read nvim API info");
    // }
    // const auto s = data.get();
    // const auto obj = msgpack::unpack(s.data(), s.size());
    // std::cout << obj.get() << std::endl;

    // auto runner = [&] -> boost::cobalt::task<void> {
    //     // co_await socket.connect();
    //     //
    //     // auto server = [&]() -> boost::asio::awaitable<void> {
    //     //     while (true) {
    //     //         // co_await socket.send("nvim_get_current_line");
    //     //         // co_await socket.send("nvim_set_current_line", "test");
    //     //         // co_await socket.send("nvim_win_get_cursor", 0);
    //     //         // co_await socket.send("nvim_win_get_cursor", 0);
    //     //         // function vim.api.nvim_buf_set_lines(buffer, start, end_, strict_indexing, replacement) end
    //     //         // co_await socket.send("nvim_buf_set_lines", 0, 0, 20, false, std::vector<std::string>{"aa",
    //     "bb"});
    //     //         // next to try:
    //     //         // co_await socket.send("nvim_buf_set_extmark", 0, 0, 20, 5,
    //     //         //                      std::vector<std::string>{std::vector<std::string>{"test"}, "O"});
    //     //
    //     //         // local mark_id = vim.api.nvim_buf_set_extmark(vim.api.nvim_get_current_buf(), ns_id, line_num,
    //     //         // col_num, {
    //     //         //   virt_lines = virt_lines,
    //     //         //   sign_text = "O",
    //     //         // })
    //     //         // co_await socket.send("nvim_get_all_options_info");
    //     //         auto timer =
    //     //             boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::seconds(5)};
    //     //         co_await timer.async_wait(boost::asio::use_awaitable);
    //     //     }
    //     // };
    //     //
    //     // auto client = [&](boost::asio::io_context& ctx) -> boost::asio::experimental::coro<void> {
    //     //     auto reader = socket.receive(ctx);
    //     //     while (auto l = co_await reader) {
    //     //         std::cout << *l << std::endl;
    //     //     }
    //     // };
    //     //
    //     // auto coro = client(ctx);
    //     // boost::asio::co_spawn(ctx, server, boost::asio::detached);
    //     // co_await coro.async_resume(boost::asio::use_awaitable);
    //     // co_await (server() && coro.async_resume(boost::asio::use_awaitable));
    // };

    // try {
    //     boost::cobalt::spawn(ctx, runner(), boost::asio::detached);
    //     const int cnt = ctx.run();
    // } catch (const std::exception& e) {
    //     std::cerr << e.what() << std::endl;
    // } catch (...) {
    //     std::cerr << "unknown error" << std::endl;
    // }
}
} // namespace rpc
