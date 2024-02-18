module;

#include <iostream>
#include <string>

export module tracker;

namespace util {

export class Tracker {
    std::string s_;

public:
    Tracker() { std::cout << "default ctor " << s_ << std::endl; }
    Tracker(std::string s) : s_(std::move(s)) {
        std::cout << "ctor " << s_ << std::endl;
    }
    Tracker(const Tracker &t) : s_(t.s_) {
        std::cout << "copy " << s_ << std::endl;
    }
    Tracker(Tracker &&t) : s_(std::move(t.s_)) {
        std::cout << "move " << s_ << std::endl;
    }

    Tracker &operator=(Tracker &&t) {
        s_ = std::move(t.s_);
        std::cout << "assign move " << s_ << std::endl;
        return *this;
    }
    Tracker &operator=(const Tracker &t) {
        s_ = t.s_;
        std::cout << "assign " << s_ << std::endl;
        return *this;
    }

    ~Tracker() { std::cout << "dtor " << s_ << std::endl; }

    const std::string &get() const { return s_; }
};
} // namespace util
