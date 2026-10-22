#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

struct Match {
    std::size_t length;
    std::size_t spaces;
};

inline constexpr std::string_view alphabet = " 0123456789abcdefghijklmnopqrstuvwxyz";

struct Node {
    std::array<std::uint32_t, alphabet.size() + 1> ascii{};
    std::unordered_map<char32_t, std::uint32_t> others;
    std::uint32_t fail = 0;
    std::vector<Match> matches;
};

struct Trie {
    std::vector<Node> nodes;
    std::array<std::uint8_t, 128> columns{};
    std::size_t max_spaces = 0;
};
