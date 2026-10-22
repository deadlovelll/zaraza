#pragma once

#include <cstddef>

struct Token {
    std::size_t start;
    std::size_t end;
    char separator;
};
