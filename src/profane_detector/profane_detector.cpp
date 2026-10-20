#include "profane_detector/profane_detector.hpp"
#include <cstddef>
#include <string>
#include <utility>

std::string ProfaneDetector::censor(const std::string &text, const char censor_char) {
    std::string normalized_text = unicode_normalizer.normalize(text);
    std::vector<Token> tokens = tokenizer.tokenize(normalized_text);
    std::vector<Token> profane_tokens = swear_word_hider.find_profane_tokens(std::move(tokens), root.get());
    if (profane_tokens.empty()) {
        return text;
    }
    std::vector<std::size_t> list_of_token_positions = swear_word_hider.token_positions(profane_tokens);
    std::vector<std::size_t> source_offsets = unicode_normalizer.map_to_source(text, list_of_token_positions);
    return swear_word_hider.hide_tokens(text, profane_tokens, source_offsets, censor_char);
}

bool ProfaneDetector::contains_profanity(const std::string &text) {
    std::vector<Token> tokens = tokenizer.tokenize(unicode_normalizer.normalize(text));
    return !swear_word_hider.find_profane_tokens(std::move(tokens), root.get()).empty();
}
