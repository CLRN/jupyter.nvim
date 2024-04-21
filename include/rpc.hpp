#pragma once

#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <msgpack.hpp>
#include <spdlog/spdlog.h>

#include <cassert>
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

    Socket(Socket&& s)
        : host_{std::move(s.host_)}
        , port_{s.port_}
        , socket_(std::move(s.socket_)) {}

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
        if constexpr (sizeof...(u)) {
            int _[] = {(pk.pack(u), 0)...};
            (void)_;
        }

        co_await socket_->async_send(boost::asio::buffer(buffer.data(), buffer.size()), boost::cobalt::use_op);
    }

    auto receive() -> boost::cobalt::generator<msgpack::object> {
        msgpack::unpacker pac;
        const auto increase = pac.buffer_capacity();

        try {
            while (socket_) {
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
public:
    Client(std::string host, std::uint16_t port)
        : socket_{Socket(std::move(host), port)} {}

    auto init() -> boost::cobalt::promise<void> {
        co_await socket_.connect();

        receive_task_.emplace(receive());

        const auto info = (co_await call("nvim_get_api_info")).as_vector();
        channel_ = info.front().as_uint64_t();
    }

    auto channel() -> std::uint32_t {
        return channel_;
    }

    template <typename... Args>
    auto call(const std::string& method, const Args&... a) -> boost::cobalt::task<msgpack::type::variant> {
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

    auto notification(std::uint32_t id) -> boost::cobalt::generator<msgpack::type::variant> {
        auto& channel = notifications_[id].emplace(128);
        auto res = co_await channel.read();
        notifications_.erase(id);
        co_return res;
    }

    auto notifications(std::uint32_t id) -> boost::cobalt::generator<msgpack::type::variant> {
        auto& channel = notifications_[id].emplace(128);

        while (channel.is_open()) {
            co_yield co_await channel.read();
        }
        co_return {};
    }

private:
    auto receive() -> boost::cobalt::promise<void> {
        auto reader = socket_.receive();
        while (reader) {
            const auto obj = co_await reader;
            if (obj.is_nil())
                break;

            const auto& message = obj.as<std::vector<msgpack::type::variant>>();
            const auto mt = static_cast<MessageType>(message[0].as_uint64_t());
            if (mt == MessageType::Response) {
                // [type, id, error, result]
                assert(message.size() == 4);
                const auto it = requests_.find(message[1].as_uint64_t());
                if (it != requests_.end()) {
                    if (message[2].is_nil()) {
                        co_await it->second->write(message[3]);
                    } else {
                        // errors are returned as array of two elements, where the message is in the end
                        auto err = message[2].as_vector().back().as_string();
                        spdlog::error("RPC call returned error: {}", err);

                        co_await it->second->write(std::make_exception_ptr(std::runtime_error(std::move(err))));
                    }
                }
            } else if (mt == MessageType::Notify) {
                // [type, message, args]
                const auto it = notifications_.find(std::stoi(message[1].as_string()));
                if (it != notifications_.end()) {
                    co_await it->second->write(message[2]);
                }
            }
        }
    }

    using ChannelDataType = std::variant<msgpack::type::variant, std::exception_ptr>;

    std::uint32_t msgid_ = 0;
    std::uint32_t channel_ = 0;
    Socket socket_;
    std::unordered_map<std::uint32_t, std::optional<boost::cobalt::channel<ChannelDataType>>> requests_;
    std::unordered_map<std::uint32_t, std::optional<boost::cobalt::channel<msgpack::type::variant>>> notifications_;
    std::optional<boost::cobalt::promise<void>> receive_task_;
};

} // namespace rpc
