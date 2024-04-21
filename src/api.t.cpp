#include "api.hpp"
#include "executor.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(API, Breathing) {
    auto& ctx = nvim::ExecutorSingleton::context();
    boost::cobalt::this_thread::set_executor(ctx.get_executor());
    auto api = nvim::Api::create("localhost", 1234);
    (void)api;
}
