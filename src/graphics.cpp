#include "graphics.hpp"
#include "geometry.hpp"
#include "api.hpp"
#include "spdlog/spdlog.h"

#include <fcntl.h>
#include <fmt/core.h>

#include <filesystem>

namespace nvim {

Graphics::Graphics(Api& api, int attempts)
    : api_{api}
    , retry_count_{attempts} {}

auto Graphics::api() -> Api& {
    return api_;
}

auto Graphics::init() -> boost::cobalt::promise<void> {
    tty_ = co_await get_tty(api_);
    ofs_.open(tty_, std::ios::binary);
    co_await update();
}

auto Graphics::update() -> boost::cobalt::promise<void> {
    struct winsize size;

    auto fd = open(tty_.c_str(), O_RDONLY | O_NOCTTY);
    ioctl(fd, TIOCGWINSZ, &size);
    close(fd);

    screen_size_ = decltype(screen_size_){};

    auto pxsize = co_await screen_size();
    terminal_size_ = Size{.w = size.ws_col, .h = size.ws_row};

    const auto [screen_px_h, screen_px_w] = pxsize;
    const auto [terminal_h, terminal_w] = terminal_size_;
    cell_size_ = Size{.w = screen_px_w ? screen_px_w / terminal_w : 1, .h = screen_px_h ? screen_px_h / terminal_h : 1};

    spdlog::info("Detected sizes, screen: {}, terminal: {}, cell: {}", pxsize, terminal_size_, cell_size_);
}

auto Graphics::run_lua_io(const std::string_view data) -> boost::cobalt::promise<std::string> {
    const int id = api_.next_notification_id();
    const std::string lua_code = fmt::format(R"(
local uv = require("luv")
local timer = uv.new_timer()
local stdin = uv.new_tty(0, true)
local stdout = uv.new_tty(1, false)
local result = ""

local function close()
  timer:stop()
  timer:close()
  stdin:close()
  uv.stop()
end

timer:start(100, 0, close)
stdin:read_start(function(err, data)
  result = data
  close()
end)

stdout:write("\x1b{2}")
stdout:close()
uv.run()
vim.fn["rpcnotify"]({0}, '{1}', result)

    )",

                                             api_.rpc_channel(), id, data);

    const std::string path = fmt::format("/tmp/nvim-cpp.{}.lua", id);
    std::shared_ptr<void> dummy{nullptr, [&path](auto) {
                                    std::filesystem::remove(path);
                                }};

    {
        std::ofstream ofs{path};
        ofs.write(lua_code.data(), lua_code.size());
    }

    auto response = api_.notification(id);
    co_await api_.nvim_exec2(fmt::format("source {}", path), {});
    const auto res = co_await response;
    co_return res.as_vector().front().as_string();
}

auto Graphics::screen_size() -> boost::cobalt::promise<Size> {
    int attempts = retry_count_;
    while (attempts && !screen_size_.w && !screen_size_.h) {
        const auto data = co_await run_lua_io("[14t");

        // clang-format off
        const auto parts = 
            data | 
            ranges::views::split(';') |
            ranges::views::transform([](auto&& rng) {
                return std::string_view(&*rng.begin(), ranges::distance(rng));
            }) |
            ranges::view::drop_exactly(1) | 
            ranges::views::transform([](auto s) { return std::atoi(s.data()); }) |
            ranges::to<std::vector<int>>();
        // clang-format on

        if (parts.size() == 2) {
            screen_size_ = Size{.w = parts.back(), .h = parts.front()};
        }

        --attempts;
    }
    co_return screen_size_;
}

auto Graphics::stream() -> std::ostream& {
    return ofs_;
}

auto Graphics::terminal_size() -> Size {
    return terminal_size_;
}

auto Graphics::cell_size() -> Size {
    return cell_size_;
}

auto Graphics::get_tty(nvim::Api& api) -> boost::cobalt::promise<std::string> {
    const auto output = co_await api.nvim_exec2("lua print(vim.fn['getpid']())", {{"output", true}});

    int pid = std::atoi(output.find("output")->second.as_string().c_str());

    std::string tty;
    while (pid) {
        std::future<std::string> data;
        boost::process::system(fmt::format("ps -p {0} -o tty=", pid), boost::process::std_out > data);
        tty = data.get();
        if (!tty.starts_with("?")) {
            if (!tty.empty()) {
                tty.pop_back();
            }
            break;
        }

        std::future<std::string> pdata;
        boost::process::system(fmt::format("ps -o ppid= {0}", pid), boost::process::std_out > pdata);
        pid = std::stoi(pdata.get());
    }

    co_return "/dev/" + tty;
}

auto Graphics::position(int win_id) -> boost::cobalt::promise<Point> {
    const auto cursor = co_await api_.nvim_win_get_cursor(win_id);
    co_await api_.nvim_win_set_cursor(win_id, {1, 0});

    int attempts = retry_count_;
    while (attempts) {
        const auto data = co_await run_lua_io("[6n");

        if (!data.empty()) {
            // clang-format off
            const auto parts = 
                data | 
                ranges::views::drop_while([](const auto c){ return c != '['; }) |
                ranges::views::drop_exactly(1) |
                ranges::views::drop_last(1) |
                ranges::views::split(';') |
                ranges::views::transform([](auto s) { return std::atoi(&*s.begin()); }) |
                ranges::to<std::vector<int>>();
            // clang-format on

            if (parts.size() >= 2) {
                co_await api_.nvim_win_set_cursor(win_id, cursor);
                co_return Point{.x = parts.back(), .y = parts.front()};
            }
        }

        --attempts;
    }

    co_return {};
}

} // namespace nvim
