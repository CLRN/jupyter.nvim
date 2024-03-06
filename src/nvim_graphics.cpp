#include "nvim_graphics.hpp"
#include "nvim.hpp"

#include <fcntl.h>
#include <fmt/core.h>

#include <filesystem>

namespace nvim {

Graphics::Graphics(Api& api)
    : api_{api} {}

auto Graphics::remote() -> boost::cobalt::promise<RemoteGraphics> {
    auto tty = co_await get_tty(api_);
    auto pxsize = co_await screen_size();

    struct winsize size;
    auto fd = open(tty.c_str(), O_RDONLY | O_NOCTTY);
    ioctl(fd, TIOCGWINSZ, &size);
    close(fd);

    co_return RemoteGraphics{std::move(tty), std::move(pxsize), {size.ws_row, size.ws_col}};
}

auto Graphics::screen_size(int attempts) -> boost::cobalt::promise<std::pair<int, int>> {
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

stdout:write("\x1b[14t")
stdout:close()
uv.run()
vim.fn["rpcnotify"]({0}, '{1}', result)
    )",

                                             api_.rpc_channel(), id);

    const std::string path = fmt::format("/tmp/nvim-cpp.{}.lua", id);

    std::shared_ptr<void> dummy{nullptr, [&path](auto) {
                                    std::filesystem::remove(path);
                                }};

    {
        std::ofstream ofs{path};
        ofs.write(lua_code.data(), lua_code.size());
    }

    while (attempts > 0) {
        auto response = api_.notification(id);
        co_await api_.nvim_exec2(fmt::format("source {}", path), {});
        const auto res = co_await response;

        // clang-format off
        const auto parts = 
            res.as_vector().front().as_string() | 
            ranges::views::split(';') |
            ranges::views::transform([](auto&& rng) {
                return std::string_view(&*rng.begin(), ranges::distance(rng));
            }) |
            ranges::view::drop_exactly(1) | 
            ranges::views::transform([](auto s) { return std::atoi(s.data()); }) |
            ranges::to<std::vector<int>>();
        // clang-format on

        if (parts.size() == 2)
            co_return {parts.front(), parts.back()};

        --attempts;
    }
    co_return {};
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

RemoteGraphics::RemoteGraphics(std::string tty, std::pair<int, int> screen_size, std::pair<int, int> terminal_size)
    : ofs_{std::move(tty), std::ios::binary}
    , screen_size_{screen_size}
    , terminal_size_{terminal_size} {
    assert(ofs_.is_open());
}

auto RemoteGraphics::screen_size() const -> std::pair<int, int> {
    return screen_size_;
}

auto RemoteGraphics::terminal_size() const -> std::pair<int, int> {
    return terminal_size_;
}
auto RemoteGraphics::stream() -> std::ostream& {
    return ofs_;
}

} // namespace nvim
