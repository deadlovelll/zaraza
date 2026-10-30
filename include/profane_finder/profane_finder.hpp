#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "censor_words_loader/censor_words_loader.hpp"
#include "deobfuscator/deobfuscator.hpp"
#include "profane_search/profane_search.hpp"
#include "source_span/source_span.hpp"
#include "swear_word_hider/swear_word_hider.hpp"
#include "taskflow/taskflow.hpp"
#include "text_chunk/text_chunk.hpp"
#include "tokenizer/tokenizer.hpp"
#include "trie_loader/trie_loader.hpp"
#include "unicode_normalizer/unicode_normalizer.hpp"

class ProfaneFinder {
public:
    ProfaneFinder(
        const std::string& lang,
        const std::string& custom_dictionary,
        const std::string& ambiguous_dictionary,
        bool deobfuscate,
        const std::string& variant_dictionary,
        const std::string& glue_dictionary,
        const std::string& substitutions
    ) : deobfuscate(deobfuscate), unicode_normalizer(deobfuscate) {
        censor_words_loader.load_censor_words(lang, custom_dictionary);
        if (!substitutions.empty()) {
            deobfuscator.load_substitutions(substitutions);
        }
        if (!glue_dictionary.empty()) {
            deobfuscator.load_glue_words(censor_words_loader.get_censor_words(), censor_words_loader.load_ranks(glue_dictionary));
        }
        if (!ambiguous_dictionary.empty()) {
            censor_words_loader.load_file(ambiguous_dictionary);
        }
        if (!variant_dictionary.empty()) {
            censor_words_loader.load_file(variant_dictionary);
        }
        std::unordered_set<std::string> words = censor_words_loader.get_censor_words();
        TrieLoader trie_loader(words);
        this->trie = trie_loader.get_trie();
    }
    std::vector<SourceSpan> find_spans(std::string_view text, tf::Executor* executor);
    bool worth_splitting(std::size_t size);
    bool contains_profanity(std::string_view text);
    std::size_t censor_inplace(std::span<char> text, tf::Executor* executor);
private:
    const std::size_t chunks_per_thread = 8;
    const std::size_t min_chunk_bytes = 256 << 10;
    const std::size_t chunk_overlap_bytes = 8 << 10;

    bool deobfuscate;
    Trie trie;
    SwearWordHider swear_word_hider;
    Deobfuscator deobfuscator;
    CensorWordsLoader censor_words_loader;
    UnicodeNormalizer unicode_normalizer;
    Tokenizer tokenizer;

    std::vector<SourceSpan> find_spans_serial(std::string_view text);
    std::vector<SourceSpan> find_spans_parallel(std::string_view text, tf::Executor& executor);
    std::vector<TextChunk> split_chunks(std::string_view text, std::size_t workers);
    void find_chunk_spans(
        std::string_view text,
        const TextChunk& chunk,
        std::vector<SourceSpan>& spans
    );
    void find_profane_tokens(
        std::string_view text,
        ProfaneSearch& search,
        bool first_only
    );
};
