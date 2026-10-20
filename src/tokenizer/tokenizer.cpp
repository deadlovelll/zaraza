#include "tokenizer/tokenizer.hpp"

#include <algorithm>
#include <utility>

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
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    const std::size_t limit = text.size();
    std::size_t token_start = from;
    bool in_token = false;
    std::size_t pos = from;
    while (pos < limit) {
        utf8proc_int32_t code_point = bytes[pos];
        utf8proc_ssize_t len = 1;
        if (code_point >= 0x80) {
            len = utf8proc_iterate(bytes + pos, static_cast<utf8proc_ssize_t>(limit - pos), &code_point);
        }
        bool word_char = len > 0 && is_word_char(static_cast<char32_t>(code_point));
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
                tokens[kept] = std::move(tokens[i]);
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
