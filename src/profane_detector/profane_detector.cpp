#include "profane_detector/profane_detector.hpp"
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "taskflow/algorithm/for_each.hpp"

std::string ProfaneDetector::censor(std::string_view text, const char censor_char) {
    std::string censored;
    if (!censor(text, censor_char, censored)) {
        return std::string(text);
    }
    return censored;
}

bool ProfaneDetector::censor(std::string_view text, const char censor_char, std::string& censored) {
    std::vector<SourceSpan> spans = profane_finder.find_spans(text, pool_for(text.size()));
    if (spans.empty()) {
        return false;
    }
    censored = swear_word_hider.hide_tokens(text, spans, censor_char);
    return true;
}

std::size_t ProfaneDetector::censor_inplace(std::span<char> text, const char censor_char) {
    return profane_finder.censor_inplace(text, censor_char, pool_for(text.size()));
}

std::size_t ProfaneDetector::censor_file(const std::filesystem::path& path, const char censor_char) {
    return file_censor.censor_file(profane_finder, path, censor_char, use_pool(executor));
}

std::vector<std::optional<std::string>> ProfaneDetector::censor_batch(
    const std::vector<std::string_view>& texts,
    const char censor_char
) {
    std::vector<std::optional<std::string>> censored(texts.size());
    tf::Executor* interop = use_pool(interop_executor);
    if (interop == nullptr) {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            censor_batch_item(texts, censor_char, censored, index);
        }
        return censored;
    }
    tf::Taskflow taskflow;
    taskflow.for_each_index(std::size_t{0}, texts.size(), std::size_t{1}, std::bind_front(
        &ProfaneDetector::censor_batch_item,
        this,
        std::cref(texts),
        censor_char,
        std::ref(censored)
    ));
    interop->run(taskflow).get();
    return censored;
}

void ProfaneDetector::censor_batch_item(
    const std::vector<std::string_view>& texts,
    const char censor_char,
    std::vector<std::optional<std::string>>& censored,
    std::size_t index
) {
    std::string censored_text;
    if (censor(texts[index], censor_char, censored_text)) {
        censored[index] = std::move(censored_text);
    }
}

std::vector<std::exception_ptr> ProfaneDetector::censor_files(
    const std::vector<std::filesystem::path>& paths,
    const char censor_char
) {
    std::vector<std::size_t> first = first_occurrences(paths);
    std::vector<std::exception_ptr> errors(paths.size());
    tf::Executor* interop = use_pool(interop_executor);
    if (interop == nullptr) {
        for (std::size_t index = 0; index < paths.size(); ++index) {
            censor_files_item(paths, first, censor_char, errors, index);
        }
    } else {
        tf::Taskflow taskflow;
        taskflow.for_each_index(std::size_t{0}, paths.size(), std::size_t{1}, std::bind_front(
            &ProfaneDetector::censor_files_item,
            this,
            std::cref(paths),
            std::cref(first),
            censor_char,
            std::ref(errors)
        ));
        interop->run(taskflow).get();
    }
    // a file listed twice is censored once and shares the result
    for (std::size_t index = 0; index < paths.size(); ++index) {
        errors[index] = errors[first[index]];
    }
    return errors;
}

void ProfaneDetector::censor_files_item(
    const std::vector<std::filesystem::path>& paths,
    const std::vector<std::size_t>& first,
    const char censor_char,
    std::vector<std::exception_ptr>& errors,
    std::size_t index
) {
    if (first[index] != index) {
        return;
    }
    try {
        censor_file(paths[index], censor_char);
    } catch (...) {
        errors[index] = std::current_exception();
    }
}

std::vector<std::size_t> ProfaneDetector::first_occurrences(const std::vector<std::filesystem::path>& paths) {
    std::map<std::filesystem::path, std::size_t> seen;
    std::vector<std::size_t> first(paths.size());
    for (std::size_t index = 0; index < paths.size(); ++index) {
        std::error_code error;
        std::filesystem::path absolute = std::filesystem::absolute(paths[index], error);
        std::filesystem::path key = error ? paths[index] : absolute.lexically_normal();
        first[index] = seen.emplace(key, index).first->second;
    }
    return first;
}

bool ProfaneDetector::contains_profanity(std::string_view text) {
    return profane_finder.contains_profanity(text);
}

void ProfaneDetector::set_num_threads(std::size_t threads) {
    if (threads == 0) {
        throw std::invalid_argument("set_num_threads: the thread count must be at least 1");
    }
    std::lock_guard<std::mutex> lock(*threads_mutex);
    if (threads_started) {
        throw std::runtime_error("set_num_threads: the thread count cannot change after parallel censoring has started");
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
    std::lock_guard<std::mutex> lock(*threads_mutex);
    if (threads_started) {
        throw std::runtime_error("set_num_interop_threads: the thread count cannot change after parallel censoring has started");
    }
    interop_executor = threads > 1 ? std::make_unique<tf::Executor>(threads) : nullptr;
}

std::size_t ProfaneDetector::get_num_interop_threads() {
    return interop_executor == nullptr ? 1 : interop_executor->num_workers();
}

// Short texts never touch the pool's lock
tf::Executor* ProfaneDetector::pool_for(std::size_t size) {
    return profane_finder.worth_splitting(size) ? use_pool(executor) : nullptr;
}

tf::Executor* ProfaneDetector::use_pool(const std::unique_ptr<tf::Executor>& pool) {
    std::lock_guard<std::mutex> lock(*threads_mutex);
    if (pool != nullptr) {
        threads_started = true;
    }
    return pool.get();
}
