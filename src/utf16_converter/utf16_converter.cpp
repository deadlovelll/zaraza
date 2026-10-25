#include "utf16_converter/utf16_converter.hpp"

#include <stdexcept>

#include "simdutf.h"

std::string Utf16Converter::to_utf8(std::span<const char> text, bool big_endian) {
    const char16_t* data = reinterpret_cast<const char16_t*>(text.data());
    std::size_t count = text.size() / 2;
    simdutf::result valid = big_endian
        ? simdutf::validate_utf16be_with_errors(data, count)
        : simdutf::validate_utf16le_with_errors(data, count);
    if (valid.error != simdutf::error_code::SUCCESS) {
        throw std::invalid_argument("censor_file: invalid UTF-16 at code unit " + std::to_string(valid.count));
    }
    std::size_t length = big_endian
        ? simdutf::utf8_length_from_utf16be(data, count)
        : simdutf::utf8_length_from_utf16le(data, count);
    std::string utf8(length, '\0');
    utf8.resize(
        big_endian
        ? simdutf::convert_valid_utf16be_to_utf8(data, count, utf8.data())
        : simdutf::convert_valid_utf16le_to_utf8(data, count, utf8.data())
    );
    return utf8;
}

std::size_t Utf16Converter::from_utf8(
    std::string_view utf8,
    std::span<char> text,
    bool big_endian
) {
    std::span<char> units = text.first(text.size() & ~std::size_t{1});
    std::size_t utf16_bytes = 2 * simdutf::utf16_length_from_utf8(utf8.data(), utf8.size());
    if (utf16_bytes > units.size()) {
        throw std::logic_error("censor_file: internal error, censored text became longer than the original");
    }
    char16_t* data = reinterpret_cast<char16_t*>(units.data());
    std::size_t size = 2 * (
        big_endian
        ? simdutf::convert_valid_utf8_to_utf16be(utf8.data(), utf8.size(), data)
        : simdutf::convert_valid_utf8_to_utf16le(utf8.data(), utf8.size(), data)
    );
    if (text.size() > units.size()) {
        text[size] = text.back();
        ++size;
    }
    return size;
}
