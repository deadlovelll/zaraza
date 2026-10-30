#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "node/node.hpp"
#include "token/token.hpp"
#include "word_filter/word_filter.hpp"

// Opt-in decoding of disguised spellings (sh1t, f*ck, fцck, fuckyou, fu ck): rewrites the normalized text in place,
// same length, so the matcher finds the dictionary word
class Deobfuscator {
public:
    void load_substitutions(const std::string& path);
    void decode(
        std::string& text,
        std::vector<Token>& tokens,
        std::size_t first,
        const Trie& trie
    );
    void load_glue_words(
        const std::unordered_set<std::string>& swear_words,
        std::unordered_map<std::string, std::size_t> ranks
    );
private:
    // From the substitutions file: one character (1 -> il1, и -> n), the same next to upside-down letters (n -> un),
    // upside-down letters, several ASCII characters (|_| -> u, longest first)
    std::unordered_map<char32_t, std::string> symbol_letters;
    std::unordered_map<char32_t, std::string> turned_symbol_letters;
    std::unordered_set<char32_t> turned_letters;
    std::vector<std::string> leet_sequences;
    std::vector<std::string> leet_sequence_letters;
    std::array<bool, 128> sequence_starts{};
    // Code points past Unicode stand for the sequences
    const char32_t leet_sequence_code = 0x110000;
    // Controls but \t to \r, ! " # $ % & ( ) * + - . / < = > ? below 64, @ [ \\ ] ^ _ ` { | } ~ DEL above
    const std::uint64_t symbol_low = 0xF000EF7EFFFFC1FE;
    const std::uint64_t symbol_high = 0xF8000001F8000001;
    const std::size_t glue_joiners = 3000;
    const std::uint64_t glue_first_multiplier = 0xC2B2AE3D27D4EB4F;
    const std::uint64_t glue_last_multiplier = 0xBF58476D1CE4E5B9;
    std::unordered_set<std::string> main_swear_words;
    Trie glue_trie;
    std::unordered_map<std::string, std::size_t> glue_ranks;
    WordFilter glue_filter;
    std::vector<std::uint64_t> glue_grams;
    std::vector<std::uint32_t> glue_short_words;

    std::uint32_t next_node(const Trie& trie, std::uint32_t node, char32_t ch);
    bool symbols_between(const std::string& text, std::size_t from, std::size_t to);
    bool is_symbol(char c);
    bool is_leading_symbol(char c);
    bool is_trailing_symbol(char c);
    bool is_mask(char c);
    bool is_weak_mask(char c);
    bool is_filler(char c);
    bool is_control(char c);
    bool is_rare_double(char c);
    bool joinable(const std::string& text, const Token& left, const Token& right);
    std::size_t reverse_overridden(std::string& text, std::vector<Token>& tokens, std::size_t first);
    bool is_direction_char(const std::string& text, std::size_t pos);
    void reverse_code_points(std::string& text, std::size_t start, std::size_t end);
    bool decode_reversed(
        std::string& text,
        std::size_t start,
        std::size_t end,
        const Trie& trie
    );
    bool is_turned(char32_t code_point);
    std::string_view turned_letters_for(char c);
    bool may_be_split(const std::string& text, const Token& left, const Token& right);
    bool is_split_word(const std::string& text, const Token& left, const Token& right);
    std::size_t join_across_gaps(
        std::string& text,
        std::vector<Token>& tokens,
        std::size_t first,
        std::size_t end
    );
    void join_tokens(std::string& text, Token& left, const Token& right);
    bool same_gap(
        const std::string& text,
        const Token& left,
        const Token& right,
        std::string_view gap
    );
    bool is_gap(char c);
    bool is_single_char(const std::string& text, const Token& token);
    bool has_marks(const std::string& text, const Token& token);
    bool has_latin_letter(const std::string& text, const Token& token);
    bool has_runs(std::uint64_t word, std::uint64_t pair, std::uint64_t valid);
    bool decode_chunk(
        std::string& text,
        std::size_t start,
        std::size_t end,
        const Trie& trie
    );
    bool may_be_glued(const std::string& text, const Token& token);
    bool decode_glued(std::string& text, Token& token);
    bool has_gram(std::uint32_t gram, std::uint64_t multiplier);
    bool is_short_swear_word(std::uint32_t gram);
    void add_gram(std::uint32_t gram, std::uint64_t multiplier);
    bool is_glue(
        const std::string& text,
        std::size_t from,
        std::size_t to,
        std::size_t limit,
        bool swear_parts
    );
    bool is_color(const std::string& text, const Token& token);
    std::size_t leet_sequence_at(const std::string& text, std::size_t pos, std::size_t end);
    std::string_view letters_for(char c);
    std::string_view letters_for(char32_t code_point);
    const Node& node_at(const Trie& trie, std::uint32_t node);
    bool ends_word(const Trie& trie, std::uint32_t node, std::size_t length);
};
