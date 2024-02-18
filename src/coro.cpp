module;

#include <coroutine>
#include <iostream>
#include <optional>
#include <utility>

#include <boost/asio/buffer.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/system/detail/error_code.hpp>

import tracker;

export module coro;

namespace coro {

template <typename T>
concept CSocket = requires(T s) {
    s.async_send(boost::asio::const_buffer(), [](boost::system::error_code) {});
    s.async_receive(boost::asio::mutable_buffer(), [](boost::system::error_code) {});
};

template <typename T>
concept CTimer =
    requires(T t) { t.async_wait([](boost::system::error_code) {}); };

template <typename T> class Task {
    struct Promise {
        Task<T> get_return_object() {
            return {std::coroutine_handle<Promise>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(T &&v) { val_ = std::move(v); }

        void unhandled_exception() {}

        std::optional<T> val_;
    };

    std::coroutine_handle<Promise> h_;

    template <CTimer Timer>
    void run_async(Timer &timer, std::coroutine_handle<> h) {
        timer.async_wait([h](boost::system::error_code ec) { h.resume(); });
    }

    template <CSocket Socket>
    void run_async(Socket &socket, std::coroutine_handle<> h) {
        socket.async_send([h](boost::system::error_code ec) { h.resume(); });
    }

public:
    using promise_type = Promise;

    Task(std::coroutine_handle<Promise> h) : h_(h) {
        // std::cout << "ctor for " << h.address() << std::endl;
    }
    ~Task() { h_.destroy(); }

    constexpr bool await_ready() const noexcept { return false; }
    T await_resume() const noexcept { return std::move(*h_.promise().val_); }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        run_async(*h_.promise().val_, h);
    }

    T run(boost::asio::io_context &ctx) const {
        while (!h_.done()) {
            ctx.poll_one();
        }
        return std::move(*h_.promise().val_);
    }
};

// Task<std::string> nested2() {
//     std::cout << "nested2" << std::endl;
//
//     co_return " nested2";
// }

Task<boost::asio::deadline_timer> nested(boost::asio::io_context &ctx) {
    // std::cout << "nested" << std::endl;
    boost::asio::deadline_timer t{ctx, boost::posix_time::seconds{5}};
    co_return t;
    // const auto one_more = []() -> Task<std::string> { co_return "lambda"; };
    // auto l = co_await one_more();
    //
    // auto t = co_await nested2();
    // std::cout << "res: " << t << std::endl;
    // co_return t + " and nested " + l;
}

Task<boost::asio::deadline_timer> f(boost::asio::io_context &ctx) {
    // std::cout << "coro" << std::endl;
    // coroutine h = nested();
    // h.resume();
    // 1. get socket
    // 2. start read to local buffer
    // 3. await for read finish
    // 4. return buffer
    auto t = co_await nested(ctx);
    // std::cout << "res: " << t << std::endl;
    co_return t;
}

// Task<util::Tracker> top() { co_return util::Tracker("top"); }

export void testCoro() {
    // const auto res = top().run();
    // std::cout << "res: " << res.get() << std::endl;
    // const auto h = f();
    boost::asio::io_context ctx;
    const auto res = f(ctx).run(ctx);

    // auto v = h.promise().;
    // std::cout << res << std::endl;
    // h.destroy();
}

} // namespace coro
