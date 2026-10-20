#include "profane_detector/profane_detector.hpp"
#include <cstddef>
#include <string>
#include <vector>

std::string ProfaneDetector::censor(const std::string &text, const char censor_char) {
    ProfaneSearch search;
    {
        std::string normalized_text = unicode_normalizer.normalize(text);
        std::vector<Token> tokens;
        for (std::size_t pos = 0; pos < normalized_text.size();) {
            pos = tokenizer.tokenize_batch(normalized_text, pos, tokens);
            swear_word_hider.find_profane_tokens(normalized_text, tokens, trie, search);
        }
    }
    if (search.profane_tokens.empty()) {
        return text;
    }
    std::vector<std::size_t> source_offsets = unicode_normalizer.map_to_source(
        text,
        swear_word_hider.token_positions(search.profane_tokens)
    );
    return swear_word_hider.hide_tokens(text, search.profane_tokens, source_offsets, censor_char);
}

bool ProfaneDetector::contains_profanity(const std::string &text) {
    std::string normalized_text = unicode_normalizer.normalize(text);
    ProfaneSearch search;
    std::vector<Token> tokens;
    for (std::size_t pos = 0; pos < normalized_text.size() && search.profane_tokens.empty();) {
        pos = tokenizer.tokenize_batch(normalized_text, pos, tokens);
        swear_word_hider.find_profane_tokens(normalized_text, tokens, trie, search);
    }
    return !search.profane_tokens.empty();
}
