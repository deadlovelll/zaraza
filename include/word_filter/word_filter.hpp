#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "token/token.hpp"

class WordFilter {
public:
    WordFilter() = default;
    WordFilter(const std::unordered_set<std::string>& words);
    std::uint64_t may_match(
        const std::string& text,
        const std::vector<Token>& tokens,
        std::size_t block
    ) const;
    bool may_contain(const std::string& text, const Token& token) const;
private:
    std::vector<std::uint64_t> word_bits;
    std::vector<std::uint64_t> pair_bits;
    int shift = 64;
    std::uint64_t word_kinds(
        const std::string& text,
        const Token& token,
        std::uint64_t& hash
    ) const;
    static std::uint64_t word_hash(
        const char* bytes,
        std::size_t start,
        std::size_t end,
        std::size_t size
    );
    std::uint64_t pair_slot(std::uint64_t first, std::uint64_t second) const;
};
