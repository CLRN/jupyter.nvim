#include "executor.hpp"
#include "handlers/images.hpp"
#include "kitty.hpp"
#include "api.hpp"
#include "graphics.hpp"
#include "printer.hpp"

#include <boost/cobalt/promise.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <ios>
#include <regex>
#include <string>

namespace jupyter {
namespace {

class Image {
    nvim::Graphics& graphics_;
    const std::string path_;
    const std::string buffer_path_;
    std::size_t line_{};
    kitty::Image image_;

    auto read_all(boost::process::async_pipe pipe) -> boost::cobalt::promise<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> buf(4096);
        boost::system::error_code e{};
        std::size_t n{};
        std::size_t total{};

        while (!e) {
            buf.resize(buf.size() * 2);
            std::tie(e, n) =
                co_await boost::asio::async_read(pipe, boost::asio::buffer(buf.data() + total, buf.size() - total),
                                                 boost::asio::as_tuple(boost::cobalt::use_op));
            total += n;
        }

        buf.resize(total);
        co_return buf;
    }

public:
    Image(nvim::Graphics& nvim, const std::string& buffer_path, const std::string& path, std::size_t line)
        : graphics_{nvim}
        , path_{path}
        , buffer_path_{buffer_path}
        , line_{line}
        , image_{nvim} {}

    auto load() -> boost::cobalt::promise<void> {
        if (path_.starts_with("http")) {
            boost::process::async_pipe ap{nvim::ExecutorSingleton::context()};

            spdlog::info("Fetching image {}", path_);

            try {
                boost::process::child process(boost::process::search_path("curl"), path_, "-o", "-", "-s",
                                              boost::process::std_out > ap);
                const auto data = co_await read_all(std::move(ap));

                spdlog::info("Got {} bytes for {}", data.size(), path_);

                image_.load(data);
            } catch (const std::exception& e) {
                spdlog::error("Failed to load image from {}, error: {}", path_, e.what());
            }
        } else {
            image_.load((boost::filesystem::path(buffer_path_).parent_path() / path_).string());
        }
    }

    auto place(int x, int y, int buf, int win_id) -> boost::cobalt::promise<void> {
        co_await image_.place(nvim::Point{.x = x, .y = static_cast<int>(y + line_ + 1)},
                              co_await nvim::Window::get(graphics_, win_id));
    }

    auto clear(int id = 0) -> void {
        image_.clear(id);
    }
};

class Buffer {
    const int id_{};
    std::vector<Image> images_;
    std::set<int> windows_;

public:
    Buffer(nvim::Graphics& remote, int id, const std::string& path)
        : id_{id}
        , images_{} {

        // read all images from the file
        std::ifstream ifs{path, std::ios::binary};
        ifs >> std::noskipws;
        const auto body = ranges::istream_view<char>(ifs) | ranges::to<std::string>();

        const std::regex link_regex(R"([^!]*!\[[^\[]*\][^\(]*\(([^\(]+)\).*)");

        // clang-format off
        images_ = ranges::views::enumerate(body | ranges::views::split('\n')) |
            ranges::views::transform([](const auto& arg) {
                const auto& [idx, rng] = arg;
                return std::make_pair(idx, std::string_view(&*rng.begin(), ranges::distance(rng)));
            }) |
            ranges::views::filter([link_regex](const auto& arg){ 
                const auto& [idx, line] = arg;
                std::cmatch match;
                return std::regex_match(line.begin(), line.end(), match, link_regex);
            }) | 
            ranges::views::transform([&](const auto& arg){ 
                const auto& [idx, line] = arg;
                std::cmatch match;
                std::regex_match(line.begin(), line.end(), match, link_regex);
                return Image{remote, path, match[1].str(), idx};
            }) | 
            ranges::to<std::vector<Image>>();
        // clang-format on
    }

    auto load() -> boost::cobalt::promise<void> {
        std::vector<boost::cobalt::promise<void>> promises;
        for (auto& im : images_) {
            promises.push_back(im.load());
        }
        co_await boost::cobalt::join(promises);
    }

    auto draw(nvim::Api& api) -> boost::cobalt::promise<void> {
        const auto win_id = co_await api.nvim_get_current_win();
        if (!windows_.insert(win_id).second)
            co_return;

        spdlog::debug("Drawing buffer {} on window {}, images {}", id_, win_id, images_.size());
        for (auto& im : images_) {
            co_await im.place(0, 0, id_, win_id);
        }
    }

    auto clear(int win_id) {
        if (windows_.erase(win_id)) {
            for (auto& im : images_) {
                im.clear(win_id);
            }
        }
    }
};

} // namespace

auto handle_markdown(nvim::Api& api, nvim::Graphics& remote, int augroup) -> boost::cobalt::promise<void> {

    std::map<int, Buffer> buffers;

    const auto handle_buffers = [&]() -> boost::cobalt::promise<void> {
        auto gen = api.nvim_create_autocmd(
            {
                "BufDelete",
                "BufEnter",
                "BufLeave",
            },
            {
                {"pattern", std::vector<nvim::Api::any>{"*.md"}},
                {"group", augroup},
            });

        while (gen) {
            auto msg = co_await gen;
            const auto data = msg.as_vector().front().as_multimap();
            const auto event = data.find("event")->second.as_string();
            const auto id = data.find("buf")->second.as_uint64_t();

            if (event == "BufEnter") {
                auto it = buffers.find(id);
                if (it == buffers.end()) {
                    spdlog::debug("New Buffer event {}", msg);
                    Buffer b{remote, static_cast<int>(id), data.find("file")->second.as_string()};
                    co_await b.load();
                    it = buffers.emplace(id, std::move(b)).first;
                }

                co_await it->second.draw(api);
            } else if (event == "BufLeave") {
                const auto it = buffers.find(id);
                if (it != buffers.end()) {
                    const auto win = co_await api.nvim_get_current_win();
                    spdlog::debug("Left buffer, window {}, data {}", win, msg);
                    it->second.clear(win);
                }
            } else {
                buffers.erase(id);
            }
        }
    };

    const auto handle_windows = [&]() -> boost::cobalt::promise<void> {
        auto gen = api.nvim_create_autocmd(
            {
                "WinClosed", "WinEnter",
                // "WinResized",
                // "WinScrolled",
            },
            {
                {"group", augroup},
            });

        while (gen) {
            auto msg = co_await gen;
            const auto data = msg.as_vector().front().as_multimap();
            const auto event = data.find("event")->second.as_string();
            const auto buf = data.find("buf")->second.as_uint64_t();

            auto it = buffers.find(buf);
            if (it == buffers.end())
                continue;

            if (event == "WinEnter") {
                spdlog::debug("Entered window {}", msg);
                co_await it->second.draw(api);
            } else if (event == "WinClosed") {
                const auto win = std::stoi(data.find("file")->second.as_string());
                spdlog::debug("Closed window {}, data {}", win, msg);
                it->second.clear(win);
            }
        }
    };

    co_await boost::cobalt::join(handle_buffers(), handle_windows());
}
} // namespace jupyter
