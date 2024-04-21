#pragma once

#include <fmt/format.h>

namespace nvim {

struct Size {
    int w{};
    int h{};
};


struct Point {
    int x{};
    int y{};
};

} // namespace nvim

template <>
class fmt::formatter<nvim::Point> {
public:
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context>
    constexpr auto format(const nvim::Point& p, Context& ctx) const {
        return fmt::format_to(ctx.out(), "({}:{})", p.x, p.y);
    }
};

template <>
class fmt::formatter<nvim::Size> {
public:
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context>
    constexpr auto format(const nvim::Size& s, Context& ctx) const {
        return fmt::format_to(ctx.out(), "({}x{})", s.w, s.h);
    }
};
