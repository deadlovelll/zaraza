#include "tokenizer/tokenizer.hpp"

#include <algorithm>
#include <bit>
#include <cstring>

#include <utf8proc.h>

std::vector<Token> Tokenizer::tokenize(const std::string& text) {
    std::vector<Token> tokens;
    for (std::size_t pos = 0; pos < text.size();) {
        pos = tokenize_batch(text, pos, tokens);
    }
    return tokens;
}

std::size_t Tokenizer::tokenize_batch(
    const std::string& text,
    std::size_t from,
    std::vector<Token>& tokens
) {
    const std::size_t batch_size = 65536;
    const std::size_t bytes_per_token_estimate = 5;
    std::size_t first = tokens.size();
    tokens.reserve(std::min(text.size() - from, batch_size) / bytes_per_token_estimate);
    std::size_t next = split_words(text, from, from + batch_size, tokens);
    join_spaced_letters(text, tokens, first);
    return next;
}

std::size_t Tokenizer::split_words(
    const std::string& text,
    std::size_t from,
    std::size_t target,
    std::vector<Token>& tokens
) {
    const std::size_t block_size = 64;
    const std::size_t max_scalar_span = 512;
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    const std::size_t limit = text.size();
    std::size_t token_start = from;
    bool in_token = false;
    std::size_t pos = from;
    std::size_t scalar_span = block_size;
    const std::uint64_t* bmp_words = word_chars().data();
    while (pos < limit) {
        std::size_t scalar_end = limit;
        if (pos + block_size <= std::min(target, limit)) {
            std::uint64_t word = 0;
            if (ascii_word_mask(bytes + pos, word)) {
                scalar_span = block_size;
                for (std::uint64_t edges = word ^ ((word << 1) | in_token); edges != 0; edges &= edges - 1) {
                    std::size_t edge = pos + static_cast<std::size_t>(std::countr_zero(edges));
                    if (in_token) {
                        tokens.push_back({token_start, edge, '\0'});
                    } else {
                        token_start = edge;
                    }
                    in_token = !in_token;
                }
                pos += block_size;
                continue;
            }
            scalar_end = std::min(pos + scalar_span, limit);
            // Back off on non-ASCII text
            scalar_span = std::min(2 * scalar_span, max_scalar_span);
        }
        while (pos < scalar_end) {
            utf8proc_int32_t code_point = bytes[pos];
            utf8proc_ssize_t len = 1;
            bool word_char = false;
            if (code_point < 0x80) [[likely]] {
                word_char = is_word_char(static_cast<char32_t>(code_point));
            } else {
                len = 0;
                if (code_point >= 0xC2 && code_point < 0xE0 && pos + 1 < limit && (bytes[pos + 1] & 0xC0) == 0x80) {
                    code_point = ((code_point & 0x1F) << 6) | (bytes[pos + 1] & 0x3F);
                    len = 2;
                } else if (
                    code_point >= 0xE0 && code_point < 0xF0 && pos + 2 < limit
                    && (bytes[pos + 1] & 0xC0) == 0x80 && (bytes[pos + 2] & 0xC0) == 0x80
                ) {
                    utf8proc_int32_t decoded = ((code_point & 0x0F) << 12) | ((bytes[pos + 1] & 0x3F) << 6) | (bytes[pos + 2] & 0x3F);
                    if (decoded >= 0x800 && (decoded < 0xD800 || decoded >= 0xE000)) {
                        code_point = decoded;
                        len = 3;
                    }
                }
                if (len == 0) {
                    len = utf8proc_iterate(bytes + pos, static_cast<utf8proc_ssize_t>(limit - pos), &code_point);
                }
                word_char = len > 0 && (
                    code_point < 0x10000 ? (bmp_words[code_point / 64] >> (code_point % 64)) & 1 : is_word_char(static_cast<char32_t>(code_point))
                );
            }
            if (word_char && !in_token) {
                token_start = pos;
                in_token = true;
            } else if (!word_char && in_token) {
                tokens.push_back({token_start, pos, '\0'});
                in_token = false;
                if (pos >= target && !is_single_letter(text, tokens.back())) {
                    return pos;
                }
            }
            pos += len > 0 ? static_cast<std::size_t>(len) : 1;
        }
    }
    if (in_token) {
        tokens.push_back({token_start, limit, '\0'});
    }
    return limit;
}

std::string Tokenizer::join(const std::string& text, const std::vector<Token>& tokens) {
    std::string words;
    for (const Token& token : tokens) {
        if (!words.empty()) {
            words += ' ';
        }
        for (std::size_t i = token.start; i < token.end; ++i) {
            if (text[i] != token.separator) {
                words += text[i];
            }
        }
    }
    return words;
}

void Tokenizer::join_spaced_letters(
    const std::string& text,
    std::vector<Token>& tokens,
    std::size_t first
) {
    std::size_t kept = first;
    std::size_t i = first;
    while (i < tokens.size()) {
        std::size_t end = i + 1;
        if (is_single_letter(text, tokens[i]) && tokens[i].end < text.size() && is_spacing_separator(text[tokens[i].end])) {
            char separator = text[tokens[i].end];
            while (
                end < tokens.size() && tokens[end].start == tokens[end - 1].end + 1
                && text[tokens[end - 1].end] == separator && is_single_letter(text, tokens[end]))
            {
                ++end;
            }
        }
        if (end - i < 3) {
            if (kept != i) {
                tokens[kept] = tokens[i];
            }
            ++kept;
            ++i;
            continue;
        }
        tokens[kept] = {tokens[i].start, tokens[end - 1].end, text[tokens[i].end]};
        ++kept;
        i = end;
    }
    tokens.resize(kept);
}

bool Tokenizer::is_word_char(char32_t code_point) {
    if (code_point < 0x80) {
        return (code_point >= U'a' && code_point <= U'z') || (code_point >= U'A' && code_point <= U'Z')
            || (code_point >= U'0' && code_point <= U'9');
    }
    utf8proc_category_t category = utf8proc_category(static_cast<utf8proc_int32_t>(code_point));
    return category >= UTF8PROC_CATEGORY_LU && category <= UTF8PROC_CATEGORY_NO;
}

bool Tokenizer::ascii_word_mask(const std::uint8_t* bytes, std::uint64_t& word) {
    static_assert(std::endian::native == std::endian::little);
    const std::uint64_t ones = 0x0101010101010101;
    const std::uint64_t high_bits = 0x80 * ones;
    std::uint64_t chunks[8];
    std::memcpy(chunks, bytes, sizeof(chunks));
    std::uint64_t seen = 0;
    for (std::uint64_t chunk : chunks) {
        seen |= chunk;
    }
    if ((seen & high_bits) != 0) {
        return false;
    }
    word = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        // Adding 0x80 - lo sets a byte's high bit when the byte is >= lo
        std::uint64_t lower = chunks[i] | 0x20 * ones;
        std::uint64_t letters = (lower + (0x80 - 'a') * ones) & ~(lower + (0x80 - 'z' - 1) * ones);
        std::uint64_t digits = (chunks[i] + (0x80 - '0') * ones) & ~(chunks[i] + (0x80 - '9' - 1) * ones);
        std::uint64_t flags = ((letters | digits) & high_bits) >> 7;
        // Gathers the low bit of every byte into the top byte
        word |= (flags * 0x0102040810204080 >> 56) << (8 * i);
    }
    return true;
}

bool Tokenizer::is_single_letter(const std::string& text, const Token& token) {
    utf8proc_int32_t code_point;
    utf8proc_ssize_t len = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t*>(text.data()) + token.start, static_cast<utf8proc_ssize_t>(token.end - token.start),
        &code_point
    );
    if (len != static_cast<utf8proc_ssize_t>(token.end - token.start)) {
        return false;
    }
    utf8proc_category_t category = utf8proc_category(code_point);
    return category >= UTF8PROC_CATEGORY_LU && category <= UTF8PROC_CATEGORY_LO;
}

bool Tokenizer::is_spacing_separator(char c) {
    return c == ' ' || c == '.' || c == '-' || c == '_' || c == '*';
}

const Tokenizer::WordChars& Tokenizer::word_chars() {
    static const WordChars chars = build_word_chars();
    return chars;
}

Tokenizer::WordChars Tokenizer::build_word_chars() {
    WordChars chars{};
    for (char32_t code_point = 0; code_point < 0x10000; ++code_point) {
        if (is_word_char(code_point)) {
            chars[code_point / 64] |= std::uint64_t{1} << (code_point % 64);
        }
    }
    return chars;
}
