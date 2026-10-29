#include <cerrno>
#include <format>
#include <system_error>

#include "censor_words_loader/censor_words_loader.hpp"

void CensorWordsLoader::load_censor_words(
    const std::string& lang, 
    const std::string& custom_dicstionary
) {
    std::string path;
    if (custom_dicstionary.empty()) {
        path = std::format("data/{}/profanity_words/profanity_wordslist.txt", lang);
    } else {
        path = custom_dicstionary;
    }
    load_file(path);
}

void CensorWordsLoader::load_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::system_error(errno, std::generic_category(), "cannot open the dictionary " + path);
    }
    std::string line;
    while (std::getline(file, line)) {
        line = unicode_normalizer.normalize(line);
        censor_words.insert(tokenizer.join(line, tokenizer.tokenize(line)));
    }
}

const std::unordered_set<std::string>& CensorWordsLoader::get_censor_words() {
    return censor_words;
}