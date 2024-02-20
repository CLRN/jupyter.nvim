module;

#include <bits/move.h>
#include <bits/stdint-uintn.h>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

export module std;

export namespace std {
using ::std::cerr;
using ::std::cout;
using ::std::endl;
using ::std::int16_t;
using ::std::int32_t;
using ::std::int64_t;
using ::std::int8_t;
using ::std::move;
using ::std::optional;
using ::std::runtime_error;
using ::std::string;
using ::std::to_string;
using ::std::uint16_t;
using ::std::uint32_t;
using ::std::uint64_t;
using ::std::uint8_t;
using ::std::vector;
using namespace ::std;

using std::coroutine_handle;
using std::coroutine_traits;
} // namespace std

export namespace std::chrono {
using std::chrono_literals::operator""ms;
}
