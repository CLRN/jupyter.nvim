module;

#include <bits/move.h>
#include <bits/stdint-uintn.h>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <iostream>
#include <string>

export module std;

export namespace std {
using ::std::cout;
using ::std::endl;
using ::std::move;
using ::std::string;
using ::std::to_string;
using ::std::uint8_t;
using namespace ::std;

using std::coroutine_handle;
using std::coroutine_traits;
} // namespace std

export namespace std::chrono {
using std::chrono_literals::operator""ms;
}
