#include "word_filter/word_filter.hpp"

#include <algorithm>
#include <bit>
#include <cstring>

WordFilter::WordFilter(const std::unordered_set<std::string>& words) {
    std::size_t slots = std::bit_ceil(std::max<std::size_t>(64, 16 * words.size()));
    word_bits.assign(slots / 32, 0);
    pair_bits.assign(slots / 64, 0);
    shift = 64 - std::countr_zero(slots);
    for (const std::string& word : words) {
        if (word.size() <= 1) continue;
        std::uint64_t previous = 0;
        for (std::size_t start = 0; start <= word.size();) {
            std::size_t end = std::min(word.find(' ', start), word.size());
            std::uint64_t hash = word_hash(word.data(), start, end, word.size());
            std::uint64_t slot = hash >> shift;
            word_bits[slot / 32] |= std::uint64_t{start == 0 && end == word.size() ? 1u : 2u} << (slot % 32 * 2);
            if (start > 0) {
                std::uint64_t pair = pair_slot(previous, hash);
                pair_bits[pair / 64] |= std::uint64_t{1} << (pair % 64);
            }
            previous = hash;
            start = end + 1;
        }
    }
}

std::uint64_t WordFilter::may_match(
    const std::string& text,
    const std::vector<Token>& tokens,
    std::size_t block
) const {
    std::uint64_t hash = 0;
    std::uint64_t kinds = word_kinds(text, tokens[block], hash);
    std::uint64_t before = 0;
    if (block > 0) {
        std::uint64_t previous = 0;
        std::uint64_t previous_kinds = word_kinds(text, tokens[block - 1], previous);
        std::uint64_t pair = pair_slot(previous, hash);
        before = ((pair_bits[pair / 64] >> (pair % 64)) | (previous_kinds | kinds) >> 2) & 1;
    }
    std::uint64_t keep = 0;
    std::size_t end = std::min(tokens.size(), block + 64);
    for (std::size_t k = block; k < end; ++k) {
        std::uint64_t next = 0;
        std::uint64_t next_kinds = k + 1 < tokens.size() ? word_kinds(text, tokens[k + 1], next) : 4;
        std::uint64_t pair = pair_slot(hash, next);
        std::uint64_t after = ((pair_bits[pair / 64] >> (pair % 64)) | (kinds | next_kinds) >> 2) & 1;
        // Single word, unknown token, or phrase word next to its pair
        keep |= ((kinds | kinds >> 2 | (kinds >> 1 & (before | after))) & 1) << (k - block);
        before = after;
        hash = next;
        kinds = next_kinds;
    }
    return keep;
}

std::uint64_t WordFilter::word_kinds(
    const std::string& text,
    const Token& token,
    std::uint64_t& hash
) const {
    if (token.separator != '\0') {
        hash = 0;
        return 4;
    }
    hash = word_hash(text.data(), token.start, token.end, text.size());
    std::uint64_t slot = hash >> shift;
    return word_bits[slot / 32] >> (slot % 32 * 2) & 3;
}

std::uint64_t WordFilter::word_hash(
    const char* bytes,
    std::size_t start,
    std::size_t end,
    std::size_t size
) {
    // Zero-padded bytes 0..8 and 8..16, the last 8 for longer words
    std::size_t length = end - start;
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    if (start + 8 <= size) {
        std::memcpy(&low, bytes + start, 8);
        low &= length >= 8 ? ~std::uint64_t{0} : ~(~std::uint64_t{0} << (8 * length));
    } else {
        std::memcpy(&low, bytes + start, length);
    }
    if (length > 8) {
        std::memcpy(&high, bytes + end - 8, 8);
        high >>= 8 * (16 - std::min<std::size_t>(length, 16));
    }
    return low * 0x9E3779B97F4A7C15 ^ high * 0xC2B2AE3D27D4EB4F;
}

std::uint64_t WordFilter::pair_slot(std::uint64_t first, std::uint64_t second) const {
    return (first * 0xBF58476D1CE4E5B9 + second) >> shift;
}
