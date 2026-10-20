#include <iostream>

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
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "no file found" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        line = unicode_normalizer.normalize(line);
        censor_words.insert(tokenizer.join(tokenizer.tokenize(line)));
    }
}

const std::unordered_set<std::string>& CensorWordsLoader::get_censor_words() {
    return censor_words;
}