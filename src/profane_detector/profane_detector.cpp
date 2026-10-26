#include "profane_detector/profane_detector.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "taskflow/algorithm/for_each.hpp"

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

std::vector<std::optional<std::string>> ProfaneDetector::censor_batch(
    const std::vector<std::string_view>& texts,
    const char censor_char,
    bool parallel
) {
    std::vector<std::optional<std::string>> censored(texts.size());
    if (interop_executor == nullptr) {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            censor_batch_item(texts, censor_char, parallel, censored, index);
        }
        return censored;
    }
    tf::Taskflow taskflow;
    taskflow.for_each_index(std::size_t{0}, texts.size(), std::size_t{1}, std::bind_front(
        &ProfaneDetector::censor_batch_item,
        this,
        std::cref(texts),
        censor_char,
        parallel,
        std::ref(censored)
    ));
    interop_executor->run(taskflow).get();
    return censored;
}

void ProfaneDetector::censor_batch_item(
    const std::vector<std::string_view>& texts,
    const char censor_char,
    bool parallel,
    std::vector<std::optional<std::string>>& censored,
    std::size_t index
) {
    std::string censored_text;
    if (censor(texts[index], censor_char, censored_text, parallel)) {
        censored[index] = std::move(censored_text);
    }
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

void ProfaneDetector::set_num_interop_threads(std::size_t threads) {
    if (threads == 0) {
        throw std::invalid_argument("set_num_interop_threads: the thread count must be at least 1");
    }
    interop_executor = threads > 1 ? std::make_unique<tf::Executor>(threads) : nullptr;
}

std::size_t ProfaneDetector::get_num_interop_threads() {
    return interop_executor == nullptr ? 1 : interop_executor->num_workers();
}
