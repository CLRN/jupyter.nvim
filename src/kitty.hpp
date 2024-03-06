#pragma once

#include <opencv2/core/mat.hpp>
#include <string>

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

public:
    Image(nvim::RemoteGraphics& nvim, const std::string& path);
    Image(Image&& im);
    ~Image();

    // places the image to a window at col x and y, accepts window width
    // and height and optional placement id
    void place(int x, int y, int w, int h, int id = 0);
    void clear(int id = 0);
};
} // namespace kitty
