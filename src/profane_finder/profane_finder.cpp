#include "profane_finder/profane_finder.hpp"

#include <utility>

std::vector<SourceSpan> ProfaneFinder::find_spans(std::string_view text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, false);
    if (search.profane_tokens.empty()) {
        return {};
    }
    return unicode_normalizer.map_to_source(text, std::move(search.profane_tokens), search.batch_starts);
}

bool ProfaneFinder::contains_profanity(std::string_view text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, true);
    return !search.profane_tokens.empty();
}

std::size_t ProfaneFinder::censor_inplace(std::span<char> text, const char censor_char) {
    std::vector<SourceSpan> spans = find_spans(std::string_view(text.data(), text.size()));
    if (spans.empty()) {
        return text.size();
    }
    return swear_word_hider.hide_tokens_inplace(text, spans, censor_char);
}

void ProfaneFinder::find_profane_tokens(
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
