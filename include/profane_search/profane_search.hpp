#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "batch_start/batch_start.hpp"
#include "tokenizer/tokenizer.hpp"

struct ProfaneSearch {
    std::uint32_t node = 0;
    std::size_t carried = 0;
    std::size_t dropped = 0;
    std::vector<Token> profane_tokens;
    std::vector<BatchStart> batch_starts;
};
