#include "profane_detector/profane_detector.hpp"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

std::string ProfaneDetector::censor(std::string_view text, const char censor_char, bool parallel) {
    std::string censored;
    if (!censor(text, censor_char, censored, parallel)) {
        return std::string(text);
    }
    return censored;
}

bool ProfaneDetector::censor(std::string_view text, const char censor_char, std::string& censored, bool parallel) {
    std::vector<SourceSpan> spans = profane_finder.find_spans(text, parallel ? executor.get() : nullptr);
    if (spans.empty()) {
        return false;
    }
    censored = swear_word_hider.hide_tokens(text, spans, censor_char);
    return true;
}

std::size_t ProfaneDetector::censor_inplace(std::span<char> text, const char censor_char, bool parallel) {
    return profane_finder.censor_inplace(text, censor_char, parallel ? executor.get() : nullptr);
}

std::size_t ProfaneDetector::censor_file(const std::filesystem::path& path, const char censor_char, bool parallel) {
    return file_censor.censor_file(profane_finder, path, censor_char, parallel ? executor.get() : nullptr);
}

bool ProfaneDetector::contains_profanity(std::string_view text) {
    return profane_finder.contains_profanity(text);
}

void ProfaneDetector::set_num_threads(std::size_t threads) {
    if (threads == 0) {
        throw std::invalid_argument("set_num_threads: the thread count must be at least 1");
    }
    executor = threads > 1 ? std::make_unique<tf::Executor>(threads) : nullptr;
}

std::size_t ProfaneDetector::get_num_threads() {
    return executor == nullptr ? 1 : executor->num_workers();
}
