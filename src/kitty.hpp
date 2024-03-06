#pragma once

#include <cstdint>
#include <string>

#include <opencv2/core/mat.hpp>

namespace nvim {
class RemoteGraphics;
}

namespace kitty {

class Cursor {
    nvim::RemoteGraphics& nvim_;
    const int x_{};
    const int y_{};

public:
    Cursor(nvim::RemoteGraphics& nvim, int x, int y);
    ~Cursor();
};

class Image {
    nvim::RemoteGraphics& nvim_;
    int id_{};
    cv::Mat image_;

    auto send();

public:
    Image(nvim::RemoteGraphics& nvim);
    Image(Image&& im);
    ~Image();

    auto load(const std::string& path) -> void;
    auto load(const std::vector<std::uint8_t>& data) -> void;

    // places the image to a window at col x and y, accepts window width
    // and height and optional placement id
    auto place(int x, int y, int w, int h, int id = 0) -> void;
    auto clear(int id = 0) -> void;
};
} // namespace kitty
