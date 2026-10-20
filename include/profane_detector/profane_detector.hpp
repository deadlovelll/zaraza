#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "censor_words_loader/censor_words_loader.hpp"
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
        this->root = trie_loader.get_trie();
    }
    std::string censor(const std::string &text, const char censor_char);
    bool contains_profanity(const std::string &text);
private:
    std::unique_ptr<Node> root;
    SwearWordHider swear_word_hider;
    CensorWordsLoader censor_words_loader;
    UnicodeNormalizer unicode_normalizer;
    Tokenizer tokenizer;
};