#pragma once

#include <cstddef>

struct TextChunk {
    std::size_t begin;
    std::size_t end;
    std::size_t owned_begin;
    std::size_t owned_end;
};
