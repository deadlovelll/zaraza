#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "file_censor/file_censor.hpp"
#include "profane_finder/profane_finder.hpp"
#include "swear_word_hider/swear_word_hider.hpp"
#include "taskflow/taskflow.hpp"

class ProfaneDetector {
public:
    ProfaneDetector(const std::string& lang, const std::string& custom_dictionary) : profane_finder(lang, custom_dictionary) {}
    std::string censor(std::string_view text, const char censor_char, bool parallel = false);
    bool censor(std::string_view text, const char censor_char, std::string& censored, bool parallel = false);
    std::size_t censor_inplace(std::span<char> text, const char censor_char, bool parallel = false);
    std::size_t censor_file(const std::filesystem::path& path, const char censor_char, bool parallel = false);
    std::vector<std::optional<std::string>> censor_batch(
        const std::vector<std::string_view>& texts,
        const char censor_char,
        bool parallel = false
    );
    bool contains_profanity(std::string_view text);
    void set_num_threads(std::size_t threads);
    std::size_t get_num_threads();
    void set_num_interop_threads(std::size_t threads);
    std::size_t get_num_interop_threads();
private:
    ProfaneFinder profane_finder;
    SwearWordHider swear_word_hider;
    FileCensor file_censor;
    std::unique_ptr<tf::Executor> executor;
    std::unique_ptr<tf::Executor> interop_executor;

    void censor_batch_item(
        const std::vector<std::string_view>& texts,
        const char censor_char,
        bool parallel,
        std::vector<std::optional<std::string>>& censored,
        std::size_t index
    );
};
