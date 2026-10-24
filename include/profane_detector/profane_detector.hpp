#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "censor_words_loader/censor_words_loader.hpp"
#include "profane_search/profane_search.hpp"
#include "trie_loader/trie_loader.hpp"
#include "swear_word_hider/swear_word_hider.hpp"
#include "tokenizer/tokenizer.hpp"
#include "unicode_normalizer/unicode_normalizer.hpp"

class ProfaneDetector {
public:
    ProfaneDetector(const std::string& lang, const std::string& custom_dictionary) {
        censor_words_loader.load_censor_words(lang, custom_dictionary);
        std::unordered_set<std::string> words = censor_words_loader.get_censor_words();
        TrieLoader trie_loader(words);
        this->trie = trie_loader.get_trie();
    }
    std::string censor(std::string_view text, const char censor_char);
    bool censor(std::string_view text, const char censor_char, std::string& censored);
    std::size_t censor_inplace(std::span<char> text, const char censor_char);
    std::size_t censor_file(const std::filesystem::path& path, const char censor_char);
    bool contains_profanity(std::string_view text);
private:
    Trie trie;
    SwearWordHider swear_word_hider;
    CensorWordsLoader censor_words_loader;
    UnicodeNormalizer unicode_normalizer;
    Tokenizer tokenizer;

    std::vector<SourceSpan> find_spans(std::string_view text);
    std::size_t censor_utf16(std::span<char> text, bool big_endian, const char censor_char);
    void find_profane_tokens(
        std::string_view text,
        ProfaneSearch& search,
        bool first_only
    );
};