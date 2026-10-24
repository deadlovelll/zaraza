#include "profane_detector/profane_detector.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mio.hpp"
#include "simdutf.h"

std::string ProfaneDetector::censor(std::string_view text, const char censor_char) {
    std::string censored;
    if (!censor(text, censor_char, censored)) {
        return std::string(text);
    }
    return censored;
}

bool ProfaneDetector::censor(std::string_view text, const char censor_char, std::string& censored) {
    std::vector<SourceSpan> spans = find_spans(text);
    if (spans.empty()) {
        return false;
    }
    censored = swear_word_hider.hide_tokens(text, spans, censor_char);
    return true;
}

std::size_t ProfaneDetector::censor_inplace(std::span<char> text, const char censor_char) {
    std::vector<SourceSpan> spans = find_spans(std::string_view(text.data(), text.size()));
    if (spans.empty()) {
        return text.size();
    }
    return swear_word_hider.hide_tokens_inplace(text, spans, censor_char);
}

std::size_t ProfaneDetector::censor_file(const std::filesystem::path& path, const char censor_char) {
    std::size_t length = static_cast<std::size_t>(std::filesystem::file_size(path));
    if (length == 0) {
        return 0;
    }
    mio::mmap_sink file(path.c_str());
    std::span<char> text(file.data(), file.size());
    std::string_view head(text.data(), std::min<std::size_t>(text.size(), 4));
    if (
        head.starts_with(std::string_view("\xFF\xFE\0\0", 4))
        || head.starts_with(std::string_view("\0\0\xFE\xFF", 4))
    ) {
        throw std::invalid_argument("censor_file: UTF-32 files are not supported");
    }
    std::size_t size = 0;
    if (head.starts_with("\xFF\xFE") || head.starts_with("\xFE\xFF")) {
        size = 2 + censor_utf16(text.subspan(2), head.starts_with("\xFE\xFF"), censor_char);
    } else {
        size = censor_inplace(text, censor_char);
    }
    file.unmap();
    if (size != length) {
        std::filesystem::resize_file(path, size);
    }
    return size;
}

std::size_t ProfaneDetector::censor_utf16(std::span<char> text, bool big_endian, const char censor_char) {
    std::span<char> units = text.first(text.size() & ~std::size_t{1});
    char16_t* data = reinterpret_cast<char16_t*>(units.data());
    std::size_t count = units.size() / 2;
    simdutf::result valid = big_endian
        ? simdutf::validate_utf16be_with_errors(data, count)
        : simdutf::validate_utf16le_with_errors(data, count);
    if (valid.error != simdutf::error_code::SUCCESS) {
        throw std::invalid_argument("censor_file: invalid UTF-16 at code unit " + std::to_string(valid.count));
    }
    std::size_t length = big_endian
        ? simdutf::utf8_length_from_utf16be(data, count)
        : simdutf::utf8_length_from_utf16le(data, count);
    std::string censored(length, '\0');
    censored.resize(big_endian
        ? simdutf::convert_valid_utf16be_to_utf8(data, count, censored.data())
        : simdutf::convert_valid_utf16le_to_utf8(data, count, censored.data()));
    censored.resize(censor_inplace(censored, censor_char));
    if (2 * simdutf::utf16_length_from_utf8(censored.data(), censored.size()) > units.size()) {
        throw std::length_error("censor_file: censored text does not fit the file");
    }
    std::size_t size = 2 * (big_endian
        ? simdutf::convert_valid_utf8_to_utf16be(censored.data(), censored.size(), data)
        : simdutf::convert_valid_utf8_to_utf16le(censored.data(), censored.size(), data));
    if (text.size() > units.size()) {
        text[size] = text.back();
        ++size;
    }
    return size;
}

bool ProfaneDetector::contains_profanity(std::string_view text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, true);
    return !search.profane_tokens.empty();
}

std::vector<SourceSpan> ProfaneDetector::find_spans(std::string_view text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, false);
    if (search.profane_tokens.empty()) {
        return {};
    }
    return unicode_normalizer.map_to_source(text, std::move(search.profane_tokens), search.batch_starts);
}

void ProfaneDetector::find_profane_tokens(
    std::string_view text,
    ProfaneSearch& search,
    bool first_only
) {
    const std::size_t lookahead = 131072;
    std::string normalized;
    std::vector<Token> tokens;
    std::size_t pos = 0;
    std::size_t from = 0;
    std::size_t ahead = lookahead;
    while (pos < text.size() || from < normalized.size()) {
        while (pos < text.size() && normalized.size() < from + ahead) {
            pos = unicode_normalizer.normalize_batch(text, pos, normalized);
            if (pos < text.size()) {
                search.batch_starts.push_back({pos, search.dropped + normalized.size()});
            }
        }
        std::size_t kept = tokens.size();
        std::size_t next = tokenizer.tokenize_batch(normalized, from, tokens);
        if (pos < text.size() && next == normalized.size()) {
            tokens.resize(kept);
            ahead *= 2;
            continue;
        }
        ahead = lookahead;
        swear_word_hider.find_profane_tokens(normalized, tokens, trie, search);
        if (first_only && !search.profane_tokens.empty()) {
            return;
        }
        from = next;
        if (pos == text.size()) {
            continue;
        }
        std::size_t cut = tokens.empty() ? next : tokens.front().start;
        normalized.erase(0, cut);
        for (Token& token : tokens) {
            token.start -= cut;
            token.end -= cut;
        }
        from -= cut;
        search.dropped += cut;
    }
}
