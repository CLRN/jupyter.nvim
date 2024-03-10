#pragma once

#include "api.hpp"

#include <boost/variant/variant.hpp>
#include <fmt/core.h>
#include <spdlog/fmt/bin_to_hex.h>

namespace nvim::detail {

struct VariantFormatter : boost::static_visitor<void> {
    fmt::format_context& ctx_;

    VariantFormatter(fmt::format_context& ctx)
        : ctx_{ctx} {}

    void operator()(const msgpack::type::nil_t& v) const {
        fmt::format_to(ctx_.out(), "nil");
    }

    void operator()(const boost::string_ref& v) const {
        fmt::format_to(ctx_.out(), "{}", std::string_view{v.data(), v.size()});
    }

    void operator()(const std::vector<char>& v) const {
        fmt::format_to(ctx_.out(), "{}", spdlog::to_hex(v.begin(), v.end()));
    }

    void operator()(const msgpack::type::raw_ref& v) const {
        fmt::format_to(ctx_.out(), "{}", spdlog::to_hex(v.str()));
    }

    void operator()(const msgpack::type::ext_ref& v) const {
        fmt::format_to(ctx_.out(), "type: {}, data: {}", v.type(), spdlog::to_hex(v.data(), v.data() + v.size()));
    }

    void operator()(const msgpack::type::ext& v) const {
        fmt::format_to(ctx_.out(), "type: {}, data: {}", v.type(), spdlog::to_hex(v.data(), v.data() + v.size()));
    }

    void operator()(const std::vector<msgpack::type::variant>& v) const {
        fmt::format_to(ctx_.out(), "{{");
        for (auto& e : v | ranges::views::drop_last(1)) {
            boost::apply_visitor(*this, e);
            fmt::format_to(ctx_.out(), ", ");
        }

        if (!v.empty()) {
            boost::apply_visitor(*this, v.back());
        }

        fmt::format_to(ctx_.out(), "}}");
    }

    template <typename T>
        requires requires(T t) {
            typename decltype(t)::key_type{};
            typename decltype(t)::mapped_type{};
        }
    void operator()(const T& v) const {
        fmt::format_to(ctx_.out(), "{{");

        const auto print = [this](const auto& kv, bool last = false) {
            const auto& [k, v] = kv;
            boost::apply_visitor(*this, k);
            fmt::format_to(ctx_.out(), "=");
            boost::apply_visitor(*this, v);
            if (!last)
                fmt::format_to(ctx_.out(), ", ");
        };

        ranges::for_each(v | ranges::views::drop_last(1), print);
        if (!v.empty()) {
            print(*v.rbegin(), true);
        }

        fmt::format_to(ctx_.out(), "}}");
    }

    template <typename T>
    void operator()(const T& v) const {
        fmt::format_to(ctx_.out(), "{}", v);
    }
};
} // namespace nvim::detail

template <>
struct fmt::formatter<nvim::Api::any> : fmt::formatter<std::string> {
    auto format(const nvim::Api::any& variant, format_context& ctx) const -> decltype(ctx.out()) {
        boost::apply_visitor(nvim::detail::VariantFormatter(ctx), variant);
        return ctx.out();
    }
};
