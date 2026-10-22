#include "profane_detector/profane_detector.hpp"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

std::string ProfaneDetector::censor(const std::string &text, const char censor_char) {
    ProfaneSearch search;
    find_profane_tokens(text, search, false);
    if (search.profane_tokens.empty()) {
        return text;
    }
    std::vector<SourceSpan> spans = unicode_normalizer.map_to_source(text, std::move(search.profane_tokens));
    return swear_word_hider.hide_tokens(text, spans, censor_char);
}

bool ProfaneDetector::contains_profanity(const std::string &text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, true);
    return !search.profane_tokens.empty();
}

void ProfaneDetector::find_profane_tokens(
    const std::string& text,
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
