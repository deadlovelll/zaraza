#pragma once

#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
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
    ProfaneDetector(
        const std::string& lang,
        const std::string& custom_dictionary,
        const std::string& ambiguous_dictionary = "",
        bool deobfuscate = false,
        const std::string& variant_dictionary = "",
        const std::string& glue_dictionary = "",
        const std::string& substitutions = ""
    ) : profane_finder(lang, custom_dictionary, ambiguous_dictionary, deobfuscate, variant_dictionary, glue_dictionary, substitutions) {}
    std::string censor(std::string_view text);
    bool censor(std::string_view text, std::string& censored);
    std::size_t censor_inplace(std::span<char> text);
    std::size_t censor_file(const std::filesystem::path& path);
    std::vector<std::optional<std::string>> censor_batch(const std::vector<std::string_view>& texts);
    std::vector<std::exception_ptr> censor_files(const std::vector<std::filesystem::path>& paths);
    bool contains_profanity(std::string_view text);
    // not thread-safe: do not call get_num_* while another thread calls set_num_*
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
    std::unique_ptr<std::mutex> threads_mutex = std::make_unique<std::mutex>();
    bool threads_started = false;

    tf::Executor* use_pool(const std::unique_ptr<tf::Executor>& pool);
    tf::Executor* pool_for(std::size_t size);
    void censor_batch_item(
        const std::vector<std::string_view>& texts,
        std::vector<std::optional<std::string>>& censored,
        std::size_t index
    );
    void censor_files_item(
        const std::vector<std::filesystem::path>& paths,
        const std::vector<std::size_t>& first,
        std::vector<std::exception_ptr>& errors,
        std::size_t index
    );
    std::vector<std::size_t> first_occurrences(const std::vector<std::filesystem::path>& paths);
};
