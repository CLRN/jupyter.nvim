#pragma once

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

    // places the image to a window at col x and y, accepts optional placement(window) id
    auto place(int x, int y, const nvim::Window& win) -> boost::cobalt::promise<void>;
    auto clear(int id = 0) -> void;
};
} // namespace kitty
