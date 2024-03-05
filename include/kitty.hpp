#pragma once

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
    std::string png_;
    int id_{};

public:
    Image(nvim::RemoteGraphics& nvim, std::string content);
    Image(Image&& im);
    ~Image();

    void place(int x, int y, int id = 0);
    void clear(int id = 0);
};
} // namespace kitty
