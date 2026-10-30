#pragma once

#include <cstddef>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "tokenizer/tokenizer.hpp"
#include "unicode_normalizer/unicode_normalizer.hpp"

class CensorWordsLoader {
    std::ifstream file;
    std::unordered_set<std::string> censor_words;
    UnicodeNormalizer unicode_normalizer;
    Tokenizer tokenizer;
public:
    void load_censor_words(const std::string& lang, const std::string& custom_dicstionary);
    void load_file(const std::string& path);
    std::unordered_map<std::string, std::size_t> load_ranks(const std::string& path);
    const std::unordered_set<std::string>& get_censor_words();
};