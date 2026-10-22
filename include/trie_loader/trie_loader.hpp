#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "node/node.hpp"

class TrieLoader {
public:
    TrieLoader(const std::unordered_set<std::string>& words)
        : nodes(1)
    {
        nodes.reserve(count_nodes(words));
        add_columns();
        add_words(words);
        build();
    }
    Trie get_trie();
private:
    std::vector<Node> nodes;
    std::array<std::uint8_t, 128> columns{};
    std::size_t max_spaces = 0;
    std::size_t count_nodes(const std::unordered_set<std::string>& words);
    void add_columns();
    void add_words(const std::unordered_set<std::string>& words);
    void build();
    std::uint8_t column(char32_t ch);
    std::uint32_t child(std::uint32_t node, char32_t ch);
    std::uint32_t fail_child(std::uint32_t node, char32_t ch);
    void link(std::uint32_t node, std::uint32_t fail);
};
