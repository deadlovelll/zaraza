#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "token/token.hpp"

class Tokenizer {
public:
    std::vector<Token> tokenize(const std::string& text);
    std::size_t tokenize_batch(
        const std::string& text,
        std::size_t from,
        std::vector<Token>& tokens
    );
    std::string join(const std::string& text, const std::vector<Token>& tokens);
private:
    std::size_t split_words(
        const std::string& text,
        std::size_t from,
        std::size_t target,
        std::vector<Token>& tokens
    );
    void join_spaced_letters(
        const std::string& text,
        std::vector<Token>& tokens,
        std::size_t first
    );
    bool ascii_word_mask(const std::uint8_t* bytes, std::uint64_t& word);
    bool is_word_char(char32_t code_point);
    bool is_single_letter(const std::string& text, const Token& token);
    bool is_spacing_separator(char c);
};
