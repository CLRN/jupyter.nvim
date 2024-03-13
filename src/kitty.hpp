#pragma once

#include "geometry.hpp"
#include "window.hpp"

#include <cstdint>
#include <string>

#include <opencv2/core/mat.hpp>

namespace nvim {
class Graphics;
}

namespace kitty {

class Cursor {
    nvim::Graphics& nvim_;
    const int x_{};
    const int y_{};

public:
    Cursor(nvim::Graphics& nvim, int x, int y);
    ~Cursor();
};

class Image {
    nvim::Graphics& nvim_;
    int id_{};
    cv::Mat image_;

    auto send();

public:
    Image(nvim::Graphics& nvim);
    Image(Image&& im);
    ~Image();

    auto load(const std::string& path) -> void;
    auto load(const std::vector<std::uint8_t>& data) -> void;
    auto area(const nvim::Window& win) const -> nvim::Size;

    // places the image to a window at col x and y, accepts optional placement(window) id
    auto place(nvim::Point where, const nvim::Window& win) const -> nvim::Size;
    auto clear(const nvim::Window& win) -> void;
};
} // namespace kitty
