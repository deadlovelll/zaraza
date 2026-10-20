#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "node/node.hpp"
#include "profane_search/profane_search.hpp"
#include "tokenizer/tokenizer.hpp"

class SwearWordHider {
public:
    void find_profane_tokens(
        const std::string& text,
        std::vector<Token>& tokens,
        const Trie& trie,
        ProfaneSearch& search
    );
    std::vector<std::size_t> token_positions(const std::vector<Token>& tokens);
    std::string hide_tokens(
        const std::string& text,
        const std::vector<Token>& tokens,
        const std::vector<std::size_t>& source_offsets,
        char censor_char
    );
private:
    std::uint32_t next_node(const std::vector<Node>& trie, std::uint32_t node, char32_t ch);
    std::size_t matched_tokens(
        const std::string& text,
        const std::vector<Token>& tokens,
        std::size_t last,
        const Match& match
    );
};