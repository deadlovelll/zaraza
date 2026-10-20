#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct Match {
    std::size_t length;
    std::size_t spaces;
};

struct Node {
    std::array<std::uint32_t, 128> ascii{};
    std::unordered_map<char32_t, std::uint32_t> others;
    std::uint32_t fail = 0;
    std::vector<Match> matches;
};
