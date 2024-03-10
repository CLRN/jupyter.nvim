#pragma once

#include <boost/asio/io_context.hpp>

namespace nvim {

class ExecutorSingleton {
public:
    static boost::asio::io_context& context() {
        static boost::asio::io_context ctx{BOOST_ASIO_CONCURRENCY_HINT_1};
        return ctx;
    }
};
} // namespace nvim
