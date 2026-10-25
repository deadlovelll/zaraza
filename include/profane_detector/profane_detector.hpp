#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "file_censor/file_censor.hpp"
#include "profane_finder/profane_finder.hpp"
#include "swear_word_hider/swear_word_hider.hpp"

class ProfaneDetector {
public:
    ProfaneDetector(const std::string& lang, const std::string& custom_dictionary) : profane_finder(lang, custom_dictionary) {}
    std::string censor(std::string_view text, const char censor_char);
    bool censor(std::string_view text, const char censor_char, std::string& censored);
    std::size_t censor_inplace(std::span<char> text, const char censor_char);
    std::size_t censor_file(const std::filesystem::path& path, const char censor_char);
    bool contains_profanity(std::string_view text);
private:
    ProfaneFinder profane_finder;
    SwearWordHider swear_word_hider;
    FileCensor file_censor;
};
