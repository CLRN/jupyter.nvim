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

enum class MessageType { Request = 0, Response = 1, Notify = 2 };

class Socket {
    const std::string host_;
    const std::uint16_t port_;
    std::optional<tcp::socket> socket_;

public:
    Socket(std::string host, std::uint16_t port)
        : host_{std::move(host)}
        , port_(port) {}

    auto close() -> void {
        if (socket_)
            socket_->close();
        socket_.reset();
    }

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
        pk.pack(static_cast<std::uint32_t>(MessageType::Request));
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
            if (obj.is_nil())
                break;

            const auto& [type, id, error, result] = obj.as<Response>();
            const auto mt = static_cast<MessageType>(type);
            if (mt == MessageType::Response) {
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
            } else if (mt == MessageType::Notify) {
                co_await notification_channel_.write(result);
            }
        }
    }

public:
    Client(std::string host, std::uint16_t port)
        : socket_{Socket(std::move(host), port)}
        , receive_task_{receive()} {}

    ~Client() {
        socket_.close();
    }

    template <typename... Args>
    auto call(const std::string& method, const Args&... a) -> boost::cobalt::task<msgpack::type::variant> {
        while (!connected_) {
            boost::asio::steady_timer tim{co_await boost::asio::this_coro::executor, std::chrono::milliseconds(100)};
            co_await tim.async_wait(boost::cobalt::use_op);
        }
        const auto id = msgid_++;

        auto& channel = requests_[id].emplace(1);

        co_await socket_.send(id, method, a...);

        auto response = co_await channel.read();
        requests_.erase(id);
        if (const auto exc = std::get_if<std::exception_ptr>(&response)) {
            std::rethrow_exception(*exc);
        } else {
            co_return std::get<msgpack::type::variant>(std::move(response));
        }
    }

    auto notifications() -> boost::cobalt::generator<msgpack::type::variant> {
        while (notification_channel_.is_open()) {
            co_yield co_await notification_channel_.read();
        }
        co_return {};
    }
};

} // namespace rpc
