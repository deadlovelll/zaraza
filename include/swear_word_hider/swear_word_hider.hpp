#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "node/node.hpp"
#include "tokenizer/tokenizer.hpp"
#include "utf8_sizer/utf8_sizer.hpp"

class SwearWordHider {
public:
    std::vector<Token> find_profane_tokens(std::vector<Token> tokens, Node* root);
    std::vector<std::size_t> token_positions(const std::vector<Token>& tokens);
    std::string hide_tokens(
        const std::string& text,
        const std::vector<Token>& tokens,
        const std::vector<std::size_t>& source_offsets,
        char censor_char
    );
private:
    UTF8Sizer utf8_sizer;

    Node* next_node(Node* node, const std::string& ch, Node* root);
    std::size_t matched_tokens(const std::vector<Token>& tokens, std::size_t last, const std::string& word);
};