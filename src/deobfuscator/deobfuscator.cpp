#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "deobfuscator/deobfuscator.hpp"
#include "trie_loader/trie_loader.hpp"

// One per line: characters, a tab, the letters they stand for, optionally a tab and turned or when_turned.
// shortcut: letters must be ASCII, the decoder rewrites in place byte for byte; others never match until that changes
void Deobfuscator::load_substitutions(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::system_error(errno, std::generic_category(), "cannot open the substitutions " + path);
    }
    std::string line;
    while (std::getline(file, line)) {
        std::size_t tab = line.find('\t');
        if (tab == 0 || tab == std::string::npos) {
            continue;
        }
        std::size_t mark_tab = line.find('\t', tab + 1);
        std::string characters = line.substr(0, tab);
        std::string letters = line.substr(tab + 1, mark_tab == std::string::npos ? std::string::npos : mark_tab - tab - 1);
        std::string mark = mark_tab == std::string::npos ? "" : line.substr(mark_tab + 1);
        unsigned char lead = static_cast<unsigned char>(characters[0]);
        if (lead < 0x80 && characters.size() > 1) {
            leet_sequences.push_back(characters);
            leet_sequence_letters.push_back(letters);
            sequence_starts[lead] = true;
            continue;
        }
        std::size_t length = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        char32_t code_point = lead < 0x80 ? lead : lead & (0x7Fu >> length);
        for (std::size_t i = 1; i < length && i < characters.size(); ++i) {
            code_point = (code_point << 6) | (static_cast<unsigned char>(characters[i]) & 0x3Fu);
        }
        (mark == "when_turned" ? turned_symbol_letters : symbol_letters)[code_point] = letters;
        if (mark == "turned") {
            turned_letters.insert(code_point);
        }
    }
}

// sh!t, f*ck, a$$hole, fuuuck: a token, or tokens joined by symbols, that spells a dictionary word
// is rewritten into that word in place, so the matcher finds it
void Deobfuscator::decode(
    std::string& text,
    std::vector<Token>& tokens,
    std::size_t first,
    const Trie& trie
) {
    const std::size_t max_edge = 3;
    const std::size_t min_marked_length = 3;
    const std::size_t max_stem = 3;
    const std::size_t max_retry_tokens = 6;
    std::size_t floor = first > 0 ? tokens[first - 1].end : 0;
    tokens.resize(reverse_overridden(text, tokens, first));
    std::size_t kept = first;
    std::size_t i = first;
    while (i < tokens.size()) {
        // Most tokens have no digits or runs inside and no symbol that could join them: a period or quote
        // before a space ends a word, only $ @ + | / \ = may start one
        const Token token = tokens[i];
        char after = token.end < text.size() ? text[token.end] : ' ';
        char beyond = token.end + 1 < text.size() ? text[token.end + 1] : ' ';
        bool symbol_after = is_symbol(after) && (is_trailing_symbol(after) || (beyond != ' ' && (beyond < '\t' || beyond > '\r')));
        if (
            !symbol_after && (token.start == 0 || !is_leading_symbol(text[token.start - 1]))
            && token.separator != '*' && (token.end - token.start < min_marked_length || !has_marks(text, token) || is_color(text, token))
        ) {
            if (kept != i) {
                tokens[kept] = token;
            }
            if (may_be_glued(text, tokens[kept])) {
                decode_glued(text, tokens[kept]);
            }
            ++kept;
            ++i;
            floor = token.end;
            continue;
        }
        std::size_t last = i;
        while (last + 1 < tokens.size() && joinable(text, tokens[last], tokens[last + 1])) {
            ++last;
        }
        std::size_t start = tokens[i].start;
        while (start > floor && tokens[i].start - start < max_edge && is_leading_symbol(text[start - 1])) {
            --start;
        }
        std::size_t end = tokens[last].end;
        std::size_t trailing_masks = 0;
        while (end < text.size() && end - tokens[last].end < max_edge && is_trailing_symbol(text[end])) {
            trailing_masks += is_mask(text[end]) ? 1 : 0;
            ++end;
        }
        // Masks after a word hide letters only after a short stem, sh**; one mask, char** and **as** are markup
        bool wrapped = tokens[i].start > 0 && is_mask(text[tokens[i].start - 1]);
        if (trailing_masks == 1 || (trailing_masks > 1 && (wrapped || tokens[last].end - tokens[last].start > max_stem))) {
            end = tokens[last].end;
        }
        // The whole run; without the symbols around, which may be markup (@f*ck, ||f*ck||); a shorter run inside
        // (sh*t...this, omg!f*ck)
        std::size_t from = i;
        std::size_t to = last;
        std::size_t merged_start = start;
        std::size_t merged_end = end;
        bool found = tokens[i].separator != ' ' && (decode_chunk(text, start, end, trie) || decode_reversed(text, start, end, trie));
        if (!found && tokens[i].separator != ' ' && (start < tokens[i].start || end > tokens[last].end)) {
            merged_start = tokens[i].start;
            merged_end = tokens[last].end;
            found = decode_chunk(text, merged_start, merged_end, trie);
        }
        for (std::size_t width = last - i; width > 0 && !found && last - i <= max_retry_tokens;) {
            --width;
            for (from = i; from + width <= last && !found;) {
                to = from + width;
                merged_start = from == i ? start : tokens[from].start;
                merged_end = to == last ? end : tokens[to].end;
                bool marked = width > 0 || has_marks(text, tokens[from]) || merged_start < tokens[from].start
                    || merged_end > tokens[to].end;
                found = marked && decode_chunk(text, merged_start, merged_end, trie);
                from += found ? 0 : 1;
            }
        }
        for (std::size_t k = i; k < (found ? from : last + 1); ++k) {
            tokens[kept] = tokens[k];
            if (may_be_glued(text, tokens[kept])) {
                decode_glued(text, tokens[kept]);
            }
            ++kept;
        }
        if (found) {
            tokens[kept] = {merged_start, merged_end, '*'};
            ++kept;
        }
        i = found ? to + 1 : last + 1;
        floor = found ? merged_end : tokens[last].end;
    }
    tokens.resize(join_across_gaps(text, tokens, first, kept));
}

// The cheap part of the check: one byte between, and both parts start like a swear word with rare first letters
bool Deobfuscator::may_be_split(const std::string& text, const Token& left, const Token& right) {
    const std::size_t max_length = 32;
    std::size_t left_length = left.end - left.start;
    std::size_t length = left_length + right.end - right.start;
    char gap = text[left.end];
    if (
        left.separator != '\0' || right.separator != '\0' || right.start != left.end + 1 || length < 4 || length > max_length
        || (gap != ' ' && gap != '-' && gap != '\n' && gap != '\t')
    ) {
        return false;
    }
    // The first 4 bytes of both parts: drop the one byte between them
    std::uint32_t gram = 0;
    if (left.start + 8 <= text.size()) {
        std::uint64_t word = 0;
        std::memcpy(&word, text.data() + left.start, 8);
        std::uint64_t kept_bytes = left_length >= 4 ? ~std::uint64_t{0} : ~(~std::uint64_t{0} << (8 * left_length));
        gram = static_cast<std::uint32_t>((word & kept_bytes) | ((word >> 8) & ~kept_bytes));
    } else {
        for (std::size_t k = 0, pos = left.start; k < 4; ++k, ++pos) {
            pos = pos == left.end ? right.start : pos;
            gram |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[pos])) << (8 * k);
        }
    }
    return has_gram(gram, glue_first_multiplier);
}

bool Deobfuscator::is_split_word(const std::string& text, const Token& left, const Token& right) {
    std::size_t length = left.end - left.start + right.end - right.start;
    // A letter counts as a known word: f uck, but not the s of he's hitting. A swear word part is found as it is:
    // fuck in, ass-cheek
    const std::string_view right_quote = "\u2019";
    if (
        left.end - left.start == 1 && left.start > 0
        && (text[left.start - 1] == '\'' || (left.start >= right_quote.size()
            && text.compare(left.start - right_quote.size(), right_quote.size(), right_quote) == 0))
    ) {
        return false;
    }
    bool left_known = left.end - left.start == 1 || glue_filter.may_contain(text, left);
    bool right_known = right.end - right.start == 1 || glue_filter.may_contain(text, right);
    if (
        (left_known && right_known) || main_swear_words.contains(text.substr(left.start, left.end - left.start))
        || main_swear_words.contains(text.substr(right.start, right.end - right.start))
    ) {
        return false;
    }
    std::uint32_t node = 0;
    for (std::size_t pos = left.start; pos < right.end; ++pos) {
        if (pos == left.end) {
            continue;
        }
        node = next_node(glue_trie, node, static_cast<char32_t>(static_cast<unsigned char>(text[pos])));
    }
    return ends_word(glue_trie, node, length);
}

// Words split by spaces or symbols: single letters with the same gap (f..u..c..k, f\nu\nc\nk, f . u . c . k), or two
// parts of a swear word with one space, dash or line break between (fu ck, afri-coon, bullsh\nit) where one part is no
// known word, so pen is and sh it stay apart
std::size_t Deobfuscator::join_across_gaps(
    std::string& text,
    std::vector<Token>& tokens,
    std::size_t first,
    std::size_t end
) {
    const std::size_t min_letters = 3;
    const std::size_t max_gap = 3;
    std::size_t kept = first;
    for (std::size_t i = first; i < end;) {
        std::size_t last = i;
        if (i + 1 < end && tokens[i].separator == '\0' && is_single_char(text, tokens[i])) {
            std::string_view gap(text.data() + tokens[i].end, tokens[i + 1].start - tokens[i].end);
            while (!gap.empty() && gap.size() <= max_gap && last + 1 < end && same_gap(text, tokens[last], tokens[last + 1], gap)) {
                ++last;
            }
        }
        if (last + 1 - i < min_letters) {
            bool split = i + 1 < end && !glue_ranks.empty() && may_be_split(text, tokens[i], tokens[i + 1])
                && is_split_word(text, tokens[i], tokens[i + 1]);
            last = split ? i + 1 : i;
        }
        Token joined = tokens[i];
        for (std::size_t k = i + 1; k <= last; ++k) {
            join_tokens(text, joined, tokens[k]);
        }
        tokens[kept] = joined;
        ++kept;
        i = last + 1;
    }
    return kept;
}

// The gap becomes '*', the separator of the joined token
void Deobfuscator::join_tokens(std::string& text, Token& left, const Token& right) {
    text.replace(left.end, right.start - left.end, right.start - left.end, '*');
    left = {left.start, right.end, '*'};
}

bool Deobfuscator::same_gap(
    const std::string& text,
    const Token& left,
    const Token& right,
    std::string_view gap
) {
    // Not initials or lists: A. S. S. Murthy, a, b, c
    if (
        right.separator != '\0' || !is_single_char(text, right) || std::string_view(text.data() + left.end, right.start - left.end) != gap
        || gap == ". " || gap == ", "
    ) {
        return false;
    }
    for (char c : gap) {
        if (!is_gap(c)) {
            return false;
        }
    }
    return true;
}

// Spaces and ASCII punctuation: f,u,c,k, f|u|c|k
bool Deobfuscator::is_gap(char c) {
    return c == ' ' || (c >= '\t' && c <= '\r') || (c > ' ' && c < '0') || (c > '9' && c < 'a') || (c > 'z' && c < 0x7F);
}

void Deobfuscator::load_glue_words(
    const std::unordered_set<std::string>& swear_words,
    std::unordered_map<std::string, std::size_t> ranks
) {
    main_swear_words = swear_words;
    glue_trie = TrieLoader(swear_words).get_trie();
    std::unordered_set<std::string> listed;
    for (const std::pair<const std::string, std::size_t>& word : ranks) {
        listed.insert(word.first);
        if (word.second < glue_joiners) {
            glue_ranks.insert(word);
        }
    }
    glue_filter = WordFilter(listed);
    // Edges many words share (fucking ends like thinking) would let common words through
    const std::size_t max_shared_edges = 5;
    std::unordered_map<std::uint32_t, std::size_t> firsts;
    std::unordered_map<std::uint32_t, std::size_t> lasts;
    for (const std::string& word : listed) {
        std::uint32_t first = 0;
        std::uint32_t last = 0;
        if (word.size() >= 4) {
            std::memcpy(&first, word.data(), 4);
            std::memcpy(&last, word.data() + word.size() - 4, 4);
            ++firsts[first];
            ++lasts[last];
        }
    }
    const std::size_t gram_words = 1024;
    glue_grams.assign(gram_words, 0);
    for (const std::string& word : main_swear_words) {
        if (word.size() < 3 || word.find(' ') != std::string::npos) {
            continue;
        }
        std::uint32_t first = 0;
        std::uint32_t last = 0;
        if (word.size() == 3) {
            std::memcpy(&first, word.data(), 3);
            glue_short_words.push_back(first);
            continue;
        }
        std::memcpy(&first, word.data(), 4);
        std::memcpy(&last, word.data() + word.size() - 4, 4);
        if (firsts[first] <= max_shared_edges) {
            add_gram(first, glue_first_multiplier);
        }
        if (lasts[last] <= max_shared_edges) {
            add_gram(last, glue_last_multiplier);
        }
    }
}

void Deobfuscator::add_gram(std::uint32_t gram, std::uint64_t multiplier) {
    std::uint64_t bit = (gram * multiplier) >> 48;
    glue_grams[bit / 64] |= std::uint64_t{1} << (bit % 64);
}

bool Deobfuscator::has_gram(std::uint32_t gram, std::uint64_t multiplier) {
    std::uint64_t bit = (gram * multiplier) >> 48;
    return (glue_grams[bit / 64] >> (bit % 64) & 1) != 0;
}

bool Deobfuscator::is_short_swear_word(std::uint32_t gram) {
    for (std::uint32_t word : glue_short_words) {
        if (word == gram) {
            return true;
        }
    }
    return false;
}

// fuckyou, gofuck: an ASCII token that starts or ends like a swear word; the bits may say yes falsely
bool Deobfuscator::may_be_glued(const std::string& text, const Token& token) {
    const std::size_t min_length = 5;
    const std::size_t max_length = 63;
    std::size_t length = token.end - token.start;
    if (glue_ranks.empty() || token.separator != '\0' || length < min_length || length > max_length) {
        return false;
    }
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    std::memcpy(&first, text.data() + token.start, 4);
    std::memcpy(&last, text.data() + token.end - 4, 4);
    return ((first | last) & 0x80808080) == 0
        && (has_gram(first, glue_first_multiplier) || has_gram(last, glue_last_multiplier)
            || is_short_swear_word(first & 0xFFFFFF) || is_short_swear_word(last >> 8));
}

// fuckyou, gofuck, fuck123: a swear word glued to common words or digits; the glued part becomes '*'.
// The list holds the joiners first, then other known words and rare ones that only look glued: cuckold
bool Deobfuscator::decode_glued(std::string& text, Token& token) {
    const std::size_t min_word = 3;
    const std::size_t short_word = 4;
    const std::size_t short_joiners = 300;
    std::size_t length = token.end - token.start;
    // Common words only are no glued swear word: #NewsHits
    if (glue_filter.may_contain(text, token) || is_glue(text, token.start, token.end, glue_joiners, false)) {
        return false;
    }
    std::uint32_t node = 0;
    for (std::size_t pos = token.start; pos < token.end; ++pos) {
        node = next_node(glue_trie, node, static_cast<char32_t>(text[pos]));
        for (const Match& match : node_at(glue_trie, node).matches) {
            std::size_t start = pos + 1 - match.length;
            if (match.spaces != 0 || match.length < min_word || match.length == length) {
                continue;
            }
            std::size_t limit = match.length >= short_word ? glue_joiners : short_joiners;
            if (!is_glue(text, token.start, start, limit, true) || !is_glue(text, pos + 1, token.end, limit, true)) {
                continue;
            }
            text.replace(token.start, start - token.start, start - token.start, '*');
            text.replace(pos + 1, token.end - pos - 1, token.end - pos - 1, '*');
            token.separator = '*';
            return true;
        }
    }
    return false;
}

// Common words (the first limit of the list), swear words if allowed and digits, one after another
bool Deobfuscator::is_glue(
    const std::string& text,
    std::size_t from,
    std::size_t to,
    std::size_t limit,
    bool swear_parts
) {
    std::uint64_t reached = 1;
    for (std::size_t i = 0; from + i < to; ++i) {
        if ((reached >> i & 1) == 0) {
            continue;
        }
        std::size_t digits = i;
        while (from + digits < to && text[from + digits] >= '0' && text[from + digits] <= '9') {
            ++digits;
        }
        reached |= std::uint64_t{1} << digits;
        for (std::size_t k = i + 1; from + k <= to; ++k) {
            std::string part = text.substr(from + i, k - i);
            std::unordered_map<std::string, std::size_t>::const_iterator rank = glue_ranks.find(part);
            if ((rank != glue_ranks.end() && rank->second < limit) || (swear_parts && main_swear_words.contains(part))) {
                reached |= std::uint64_t{1} << k;
            }
        }
    }
    return (reached >> (to - from) & 1) != 0;
}

// RLO (U+202E) shows the letters after it backwards up to LRO, PDF, PDI or a line end: kcuf reads fuck.
// Words split only by these characters are joined: fu<RLO>gnikc
std::size_t Deobfuscator::reverse_overridden(std::string& text, std::vector<Token>& tokens, std::size_t first) {
    const std::string_view right_to_left = "\u202E";
    std::size_t from = first > 0 ? tokens[first - 1].end : 0;
    bool found = false;
    for (std::size_t pos = from; first < tokens.size() && pos < tokens.back().end && !found; ++pos) {
        const void* lead = std::memchr(text.data() + pos, 0xE2, tokens.back().end - pos);
        pos = lead == nullptr ? tokens.back().end : static_cast<std::size_t>(static_cast<const char*>(lead) - text.data());
        found = lead != nullptr && is_direction_char(text, pos);
    }
    if (!found) {
        return tokens.size();
    }
    bool reversed = false;
    std::size_t kept = first;
    for (std::size_t i = first; i < tokens.size(); ++i) {
        bool direction_gap = tokens[i].start > from;
        for (std::size_t pos = from; pos < tokens[i].start;) {
            bool direction = is_direction_char(text, pos);
            if (direction || text[pos] == '\n') {
                reversed = direction && text.compare(pos, right_to_left.size(), right_to_left) == 0;
            }
            direction_gap = direction_gap && direction;
            pos += direction ? right_to_left.size() : 1;
        }
        if (reversed) {
            reverse_code_points(text, tokens[i].start, tokens[i].end);
        }
        if (kept > first && direction_gap && tokens[kept - 1].separator != ' ' && tokens[i].separator == '\0') {
            join_tokens(text, tokens[kept - 1], tokens[i]);
        } else {
            tokens[kept] = tokens[i];
            ++kept;
        }
        from = tokens[i].end;
    }
    return kept;
}

// The normalizer keeps only these: PDF, LRO, RLO (E2 80 AC..AE) and PDI (E2 81 A9)
bool Deobfuscator::is_direction_char(const std::string& text, std::size_t pos) {
    if (pos + 3 > text.size() || static_cast<unsigned char>(text[pos]) != 0xE2) {
        return false;
    }
    unsigned char second = static_cast<unsigned char>(text[pos + 1]);
    unsigned char third = static_cast<unsigned char>(text[pos + 2]);
    return (second == 0x80 && third >= 0xAC && third <= 0xAE) || (second == 0x81 && third == 0xA9);
}

void Deobfuscator::reverse_code_points(std::string& text, std::size_t start, std::size_t end) {
    std::reverse(text.begin() + static_cast<std::ptrdiff_t>(start), text.begin() + static_cast<std::ptrdiff_t>(end));
    // Each character's bytes are backwards now: continuation bytes, then the lead byte
    for (std::size_t pos = start; pos < end; ++pos) {
        std::size_t lead = pos;
        while (lead < end && (static_cast<unsigned char>(text[lead]) & 0xC0) == 0x80) {
            ++lead;
        }
        if (lead > pos && lead < end) {
            std::reverse(text.begin() + static_cast<std::ptrdiff_t>(pos), text.begin() + static_cast<std::ptrdiff_t>(lead) + 1);
            pos = lead;
        }
    }
}

// Upside-down text is also written backwards: ʞɔnɟ
bool Deobfuscator::decode_reversed(
    std::string& text,
    std::size_t start,
    std::size_t end,
    const Trie& trie
) {
    bool turned = false;
    for (std::size_t pos = start; pos < end && !turned;) {
        unsigned char lead = static_cast<unsigned char>(text[pos]);
        std::size_t len = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        char32_t code_point = lead < 0x80 ? lead : lead & (0x7Fu >> len);
        for (std::size_t j = 1; j < len && pos + j < end; ++j) {
            code_point = (code_point << 6) | (static_cast<unsigned char>(text[pos + j]) & 0x3Fu);
        }
        turned = is_turned(code_point);
        pos += len;
    }
    if (!turned) {
        return false;
    }
    reverse_code_points(text, start, end);
    if (decode_chunk(text, start, end, trie)) {
        return true;
    }
    reverse_code_points(text, start, end);
    return false;
}

// Fillers and dashes alone join only a single letter: f.u_ck, f-ck, but who.re, hand_job and ant-man stay two words
bool Deobfuscator::joinable(const std::string& text, const Token& left, const Token& right) {
    // Question marks in a row end a question: my dog??it
    if (
        left.separator == ' ' || right.separator == ' ' || !symbols_between(text, left.end, right.start)
        || (right.start - left.end >= 2 && text[left.end] == '?' && text[left.end + 1] == '?')
    ) {
        return false;
    }
    for (std::size_t i = left.end; i < right.start; ++i) {
        if (!is_filler(text[i]) && text[i] != '-') {
            return true;
        }
    }
    return is_single_char(text, left) || is_single_char(text, right);
}

// #bada55 is a color, not badass
bool Deobfuscator::is_color(const std::string& text, const Token& token) {
    std::size_t length = token.end - token.start;
    if (token.start == 0 || text[token.start - 1] != '#' || (length != 3 && length != 6 && length != 8)) {
        return false;
    }
    for (std::size_t i = token.start; i < token.end; ++i) {
        if ((text[i] < '0' || text[i] > '9') && (text[i] < 'a' || text[i] > 'f')) {
            return false;
        }
    }
    return true;
}

bool Deobfuscator::is_single_char(const std::string& text, const Token& token) {
    unsigned char lead = static_cast<unsigned char>(text[token.start]);
    std::size_t len = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
    return token.end - token.start == len;
}

// Up to 3 symbols, or up to 6 stars and hashes: f*****g
bool Deobfuscator::symbols_between(const std::string& text, std::size_t from, std::size_t to) {
    const std::size_t max_gap = 3;
    const std::size_t max_masked_gap = 6;
    if (to <= from || to - from > max_masked_gap) {
        return false;
    }
    bool masked = true;
    for (std::size_t i = from; i < to; ++i) {
        if (!is_symbol(text[i])) {
            return false;
        }
        masked = masked && (text[i] == '*' || text[i] == '#');
    }
    return to - from <= max_gap || masked;
}

// Before a word: $hit, @ss, |\|igger, /\ss. Masks there wrap words as markup: #or#, *word*
bool Deobfuscator::is_leading_symbol(char c) {
    return c == '$' || c == '@' || c == '+' || c == '|' || c == '/' || c == '\\' || c == '=';
}

// After a word: a$$, shi+, sh**, fuc|<
bool Deobfuscator::is_trailing_symbol(char c) {
    return c == '$' || c == '+' || c == '*' || c == '#' || c == '|' || c == '<' || c == '{' || c == '(';
}

// Between letters: sh!t, f*ck, f-ck, a$$hole, f.u_ck, fagg()t; one bit per ASCII symbol
bool Deobfuscator::is_symbol(char c) {
    unsigned char byte = static_cast<unsigned char>(c);
    return byte < 128 && ((byte < 64 ? symbol_low >> byte : symbol_high >> (byte - 64)) & 1) != 0;
}

// A hidden letter. '-' and '?' only inside a word, and '-' hides only a vowel: f-ck, but who-e is no whore
bool Deobfuscator::is_mask(char c) {
    return c == '*' || c == '#' || c == '-' || c == '?';
}

// Symbols that hide one letter only between letters of a longer word: f&ck, sh^t, but A&S is no word.
// Not = or %, which code and date formats put between letters: t=rd, %H%M%S
bool Deobfuscator::is_weak_mask(char c) {
    return c == '^' || c == '`' || c == '"' || c == '&';
}

// Control characters inside a word: lez\x13bo
bool Deobfuscator::is_control(char c) {
    unsigned char byte = static_cast<unsigned char>(c);
    return (byte > 0 && byte < '\t') || (byte > '\r' && byte < ' ') || byte == 0x7F;
}

bool Deobfuscator::is_filler(char c) {
    return c == '.' || c == '_' || c == '~';
}

// Three equal bytes in a row, or a rare double: h-j, q or u-y twice
bool Deobfuscator::has_runs(std::uint64_t word, std::uint64_t pair, std::uint64_t valid) {
    const std::uint64_t ones = 0x0101010101010101;
    const std::uint64_t high_bits = 0x80 * ones;
    std::uint64_t same = pair | (word ^ (word >> 16));
    std::uint64_t low = word & ~high_bits;
    std::uint64_t rare = ((low + (0x80 - 'h') * ones) & ~(low + (0x80 - 'j' - 1) * ones))
        | ((low + (0x80 - 'q') * ones) & ~(low + (0x80 - 'q' - 1) * ones))
        | ((low + (0x80 - 'u') * ones) & ~(low + (0x80 - 'y' - 1) * ones));
    std::uint64_t doubled = ~(((pair & ~high_bits) + ~high_bits) | pair);
    return ((same - ones) & ~same & high_bits & (valid >> 16)) != 0 || (doubled & rare & ~word & high_bits & (valid >> 8)) != 0;
}

// Letters words almost never double, so fuuck and shiit are stretched; assess is not asses
bool Deobfuscator::is_rare_double(char c) {
    return (c >= 'h' && c <= 'j') || c == 'q' || (c >= 'u' && c <= 'y');
}

// Digits next to letters, three equal bytes in a row or a rare double: the token may be leet or stretched
bool Deobfuscator::has_marks(const std::string& text, const Token& token) {
    const std::uint64_t ones = 0x0101010101010101;
    const std::uint64_t high_bits = 0x80 * ones;
    std::uint64_t digits = 0;
    std::uint64_t letters = 0;
    std::uint64_t foreign = 0;
    std::uint64_t upper = 0;
    // A window of 8 bytes checks runs starting in its first 6
    for (std::size_t pos = token.start; pos < token.end; pos += 6) {
        std::size_t length = std::min<std::size_t>(8, token.end - pos);
        std::uint64_t word = 0;
        if (pos + 8 <= text.size()) {
            std::memcpy(&word, text.data() + pos, 8);
        } else {
            std::memcpy(&word, text.data() + pos, length);
        }
        std::uint64_t valid = length == 8 ? ~std::uint64_t{0} : ~(~std::uint64_t{0} << (8 * length));
        word &= valid;
        std::uint64_t low = word & ~high_bits;
        digits |= (low + (0x80 - '0') * ones) & ~(low + (0x80 - '9' - 1) * ones) & ~word & high_bits & valid;
        letters |= (word | ((low + (0x80 - 'a') * ones) & ~(low + (0x80 - 'z' - 1) * ones))) & high_bits & valid;
        foreign |= word & high_bits;
        upper |= (low + (0x80 - 'A') * ones) & ~(low + (0x80 - 'Z' - 1) * ones) & ~word & high_bits & valid;
        // Zero where a byte equals the next one
        std::uint64_t pair = word ^ (word >> 8);
        if (((pair - ones) & ~pair & high_bits & (valid >> 8)) != 0 && has_runs(word, pair, valid)) {
            return true;
        }
        if (length < 8) {
            break;
        }
    }
    return (digits != 0 && letters != 0) || upper != 0 || (foreign != 0 && has_latin_letter(text, token));
}

// Letters no Cyrillic one normalizes to: fцck is disguised, роги (porи) is Russian
bool Deobfuscator::has_latin_letter(const std::string& text, const Token& token) {
    for (std::size_t i = token.start; i < token.end; ++i) {
        if (std::string_view("dfgijlnqsuvz").find(text[i]) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

// Digits and symbols become letters (sh1t, a$$), a mask any letter (f*ck), fillers are skipped (f.u_ck),
// a run of three or more or a rare double one or two letters (fuuuck, fuuck). Only a whole dictionary word counts
bool Deobfuscator::decode_chunk(
    std::string& text,
    std::size_t start,
    std::size_t end,
    const Trie& trie
) {
    const std::size_t max_units = 32;
    const std::size_t max_steps = 4096;
    const std::size_t max_masks = 2;
    const std::size_t max_wrapped_masks = 6;
    const std::size_t max_fillers = 3;
    std::array<std::size_t, max_units + 1> unit_starts;
    std::array<std::size_t, max_units + 1> run_index;
    std::array<char32_t, max_units> code_points;
    std::size_t units = 0;
    std::size_t letters = 0;
    std::size_t substitutes = 0;
    std::size_t mask_count = 0;
    std::size_t filler_count = 0;
    std::size_t digit_count = 0;
    std::size_t weak_masks = 0;
    std::size_t digit_streak = 0;
    bool long_number = false;
    bool stretch = false;
    bool turned = false;
    for (std::size_t pos = start; pos < end;) {
        if (units == max_units) {
            return false;
        }
        std::size_t sequence = leet_sequence_at(text, pos, end);
        if (sequence < leet_sequences.size()) {
            ++substitutes;
            code_points[units] = leet_sequence_code + static_cast<char32_t>(sequence);
            run_index[units] = 0;
            unit_starts[units] = pos;
            ++units;
            pos += leet_sequences[sequence].size();
            continue;
        }
        unsigned char lead = static_cast<unsigned char>(text[pos]);
        std::size_t len = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        char32_t code_point = lead < 0x80 ? lead : lead & (0x7Fu >> len);
        for (std::size_t j = 1; j < len; ++j) {
            code_point = (code_point << 6) | (static_cast<unsigned char>(text[pos + j]) & 0x3Fu);
        }
        // Masks hide one letter each, f***ing, so they never form a run
        bool repeat = units > 0 && code_points[units - 1] == code_point && !is_mask(static_cast<char>(lead))
            && !is_weak_mask(static_cast<char>(lead));
        if ((lead >= 'a' && lead <= 'z') || (lead >= 0x80 && letters_for(code_point).empty())) {
            ++letters;
        } else if (is_mask(static_cast<char>(lead))) {
            ++mask_count;
        } else if (is_weak_mask(static_cast<char>(lead))) {
            ++mask_count;
            ++weak_masks;
        } else if (is_filler(static_cast<char>(lead)) || is_control(static_cast<char>(lead))) {
            ++filler_count;
        } else if (!repeat) {
            // A run of one substitute is one letter: fu((((k
            ++substitutes;
            digit_count += lead >= '0' && lead <= '9' ? 1 : 0;
        }
        digit_streak = lead >= '0' && lead <= '9' ? digit_streak + 1 : 0;
        long_number = long_number || digit_streak >= 3;
        run_index[units] = repeat ? run_index[units - 1] + 1 : 0;
        stretch = stretch || run_index[units] == 2 || (run_index[units] == 1 && is_rare_double(static_cast<char>(lead)));
        code_points[units] = code_point;
        unit_starts[units] = pos;
        turned = turned || is_turned(code_point);
        ++units;
        pos += len;
    }
    unit_starts[units] = end;
    run_index[units] = 0;
    // Digits next to two letters are mostly names of things: F-4Ss, AS.5. Symbols alone need one letter: a$$
    bool symbols_only = digit_count == 0 && mask_count == 0 && filler_count == 0 && substitutes > 0;
    bool inner_masks = !is_mask(static_cast<char>(text[start])) && !is_mask(static_cast<char>(text[unit_starts[units - 1]]));
    // Letters on both ends fix the length of a longer mask: f*****g, b******t
    std::size_t mask_limit = !inner_masks ? max_masks : letters >= 2 ? max_wrapped_masks : max_masks;
    // Mostly leet: $h1+, 5h17. Four letters and substitutes at least, no number (1380s, Sh1.7) and a word of the
    // main list, so a55 and AS.5 stay
    const std::size_t min_leet_units = 4;
    bool leet_heavy = letters >= 1 && letters + substitutes >= min_leet_units && weak_masks == 0 && filler_count == 0
        && !long_number && (letters < substitutes || (digit_count > 0 && letters < 3));
    if (
        (letters < (symbols_only ? 1 : 2) && !leet_heavy) || (!symbols_only && letters < substitutes && !leet_heavy)
        || mask_count > mask_limit || filler_count > max_fillers || (digit_count > 0 && letters < 3 && !leet_heavy)
        || (weak_masks > 0 && letters < 3) || (substitutes + mask_count == 0 && filler_count < 2 && !stretch)
    ) {
        return false;
    }
    // Options of each unit: its letters, then itself if not ASCII, then skipping it
    std::array<std::string_view, max_units> options;
    std::array<bool, max_units> literal;
    std::array<bool, max_units> skippable;
    for (std::size_t k = 0; k < units; ++k) {
        char c = text[unit_starts[k]];
        bool ascii = code_points[k] < 0x80;
        bool extra = run_index[k] >= 2;
        bool stretched = run_index[k] == 1 && (run_index[k + 1] == 2 || is_rare_double(c));
        // More than max_masks masks fix the word's length: each one is a letter
        bool spacer = ascii && ((is_mask(c) && mask_count <= max_masks + 1) || is_filler(c) || is_control(c) || (c >= 'A' && c <= 'Z'));
        options[k] = extra ? std::string_view() : !ascii ? letters_for(code_points[k]) : turned ? turned_letters_for(c) : letters_for(c);
        literal[k] = !extra && !ascii && code_points[k] < leet_sequence_code;
        skippable[k] = extra || stretched || spacer;
    }
    std::array<std::uint32_t, max_units + 1> nodes;
    std::array<std::size_t, max_units + 1> lengths;
    std::array<std::size_t, max_units + 1> choices;
    nodes[0] = 0;
    lengths[0] = 0;
    choices[0] = 0;
    std::size_t k = 0;
    bool found = false;
    for (std::size_t steps = 0; steps < max_steps && !found; ++steps) {
        if (k == units) {
            found = ends_word(trie, nodes[k], lengths[k]);
            // Masks and heavy leet spell only words of the main list: f**k, but br***t is no misspelling brdsht
            if (found && (mask_count > 0 || leet_heavy) && !main_swear_words.empty()) {
                std::string spelled;
                for (std::size_t unit = 0; unit < units; ++unit) {
                    if (choices[unit] < options[unit].size()) {
                        spelled += options[unit][choices[unit]];
                    } else if (literal[unit] && choices[unit] == options[unit].size()) {
                        spelled.append(text, unit_starts[unit], unit_starts[unit + 1] - unit_starts[unit]);
                    }
                }
                found = main_swear_words.contains(spelled);
            }
            if (!found) {
                --k;
                ++choices[k];
            }
            continue;
        }
        std::size_t spelled = options[k].size() + (literal[k] ? 1 : 0);
        if (choices[k] >= spelled + (skippable[k] ? 1 : 0)) {
            if (k == 0) {
                break;
            }
            --k;
            ++choices[k];
            continue;
        }
        if (choices[k] < spelled) {
            bool letter = choices[k] < options[k].size();
            char32_t ch = letter ? static_cast<char32_t>(options[k][choices[k]]) : code_points[k];
            std::uint32_t next = next_node(trie, nodes[k], ch);
            std::size_t length = lengths[k] + (letter ? 1 : unit_starts[k + 1] - unit_starts[k]);
            // A fail transition means the spelling left the dictionary's prefixes
            if (node_at(trie, next).depth != length) {
                ++choices[k];
                continue;
            }
            nodes[k + 1] = next;
            lengths[k + 1] = length;
        } else {
            nodes[k + 1] = nodes[k];
            lengths[k + 1] = lengths[k];
        }
        ++k;
        choices[k] = 0;
    }
    if (!found) {
        return false;
    }
    // Same length: a chosen letter takes the unit's first byte, '*' (the token's separator) the rest
    for (std::size_t unit = 0; unit < units; ++unit) {
        std::size_t pos = unit_starts[unit];
        std::size_t len = unit_starts[unit + 1] - pos;
        if (choices[unit] < options[unit].size()) {
            text[pos] = options[unit][choices[unit]];
            text.replace(pos + 1, len - 1, len - 1, '*');
        } else if (!literal[unit] || choices[unit] > options[unit].size()) {
            text.replace(pos, len, len, '*');
        }
    }
    return true;
}

std::size_t Deobfuscator::leet_sequence_at(const std::string& text, std::size_t pos, std::size_t end) {
    unsigned char lead = static_cast<unsigned char>(text[pos]);
    for (std::size_t i = 0; lead < 0x80 && sequence_starts[lead] && i < leet_sequences.size(); ++i) {
        const std::string& sequence = leet_sequences[i];
        if (sequence[0] == text[pos] && end - pos >= sequence.size() && std::string_view(text.data() + pos, sequence.size()) == sequence) {
            return i;
        }
    }
    return leet_sequences.size();
}

std::string_view Deobfuscator::letters_for(char c) {
    std::string_view letters = letters_for(static_cast<char32_t>(static_cast<unsigned char>(c)));
    if (!letters.empty()) {
        return letters;
    }
    if (is_mask(c) || is_weak_mask(c)) {
        return alphabet.substr(alphabet.find('a'));
    }
    // A letter or another digit spells itself
    std::size_t column = alphabet.find(c);
    return column == std::string_view::npos || c == ' ' ? std::string_view() : alphabet.substr(column, 1);
}

std::string_view Deobfuscator::letters_for(char32_t code_point) {
    if (code_point >= leet_sequence_code) {
        return leet_sequence_letters[code_point - leet_sequence_code];
    }
    std::unordered_map<char32_t, std::string>::const_iterator letters = symbol_letters.find(code_point);
    return letters == symbol_letters.end() ? std::string_view() : std::string_view(letters->second);
}

bool Deobfuscator::is_turned(char32_t code_point) {
    return code_point >= 0x80 && turned_letters.contains(code_point);
}

// Next to upside-down letters, n is a turned u: ɟnɔʞ
std::string_view Deobfuscator::turned_letters_for(char c) {
    std::unordered_map<char32_t, std::string>::const_iterator letters = turned_symbol_letters.find(static_cast<unsigned char>(c));
    return letters == turned_symbol_letters.end() ? letters_for(c) : std::string_view(letters->second);
}

const Node& Deobfuscator::node_at(const Trie& trie, std::uint32_t node) {
    return *reinterpret_cast<const Node*>(reinterpret_cast<const char*>(trie.nodes.data()) + node);
}

bool Deobfuscator::ends_word(const Trie& trie, std::uint32_t node, std::size_t length) {
    for (const Match& match : node_at(trie, node).matches) {
        if (match.spaces == 0 && match.length == length) {
            return true;
        }
    }
    return false;
}

std::uint32_t Deobfuscator::next_node(const Trie& trie, std::uint32_t node, char32_t ch) {
    const char* base = reinterpret_cast<const char*>(trie.nodes.data());
    if (ch < trie.columns.size()) {
        return *reinterpret_cast<const std::uint32_t*>(base + sizeof(std::uint32_t) * trie.columns[ch] + node);
    }
    for (;;) {
        const Node& current = *reinterpret_cast<const Node*>(base + node);
        std::unordered_map<char32_t, std::uint32_t>::const_iterator child = current.others.find(ch);
        if (child != current.others.end()) {
            return child->second;
        }
        if (node == 0) {
            return 0;
        }
        node = current.fail;
    }
}
