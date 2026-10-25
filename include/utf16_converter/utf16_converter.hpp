#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

class Utf16Converter {
public:
    std::string to_utf8(std::span<const char> text, bool big_endian);
    std::size_t from_utf8(
        std::string_view utf8,
        std::span<char> text,
        bool big_endian
    );
};
