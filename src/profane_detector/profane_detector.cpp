#include "profane_detector/profane_detector.hpp"
#include <string>
#include <string_view>
#include <vector>

std::string ProfaneDetector::censor(std::string_view text, const char censor_char) {
    std::string censored;
    if (!censor(text, censor_char, censored)) {
        return std::string(text);
    }
    return censored;
}

bool ProfaneDetector::censor(std::string_view text, const char censor_char, std::string& censored) {
    std::vector<SourceSpan> spans = profane_finder.find_spans(text);
    if (spans.empty()) {
        return false;
    }
    censored = swear_word_hider.hide_tokens(text, spans, censor_char);
    return true;
}

std::size_t ProfaneDetector::censor_inplace(std::span<char> text, const char censor_char) {
    return profane_finder.censor_inplace(text, censor_char);
}

std::size_t ProfaneDetector::censor_file(const std::filesystem::path& path, const char censor_char) {
    return file_censor.censor_file(profane_finder, path, censor_char);
}

bool ProfaneDetector::contains_profanity(std::string_view text) {
    return profane_finder.contains_profanity(text);
}
