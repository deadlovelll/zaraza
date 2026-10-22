#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "swear_word_hider/swear_word_hider.hpp"

#include <utf8proc.h>

void SwearWordHider::find_profane_tokens(
    const std::string& text,
    std::vector<Token>& tokens,
    const Trie& trie,
    ProfaneSearch& search
) {
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    std::uint32_t node = search.node;
    for (std::size_t k = search.carried; k < tokens.size(); ++k) {
        node = next_node(trie, node, U' ');
        const Token token = tokens[k];
        for (std::size_t i = token.start; i < token.end;) {
            if (bytes[i] == static_cast<utf8proc_uint8_t>(token.separator)) {
                ++i;
                continue;
            }
            if (bytes[i] < 0x80) {
                node = next_node(trie, node, bytes[i]);
                ++i;
                continue;
            }
            // Inline UTF-8 decode, tokenizer already validated it
            std::size_t len = bytes[i] >= 0xF0 ? 4 : bytes[i] >= 0xE0 ? 3 : 2;
            // Lead byte bits, then 6 bits per continuation byte
            char32_t code_point = bytes[i] & (0x7Fu >> len);
            for (std::size_t j = 1; j < len; ++j) {
                code_point = (code_point << 6) | (bytes[i + j] & 0x3Fu);
            }
            node = next_node(trie, node, code_point);
            i += len;
        }
        for (const Match& match : trie.nodes[node].matches) {
            std::size_t matched = matched_tokens(text, tokens, k, match);
            if (matched == 0) {
                continue;
            }
            while (!search.profane_tokens.empty() && search.profane_tokens.back().start >= tokens[k + 1 - matched].start) {
                search.profane_tokens.pop_back();
            }
            for (std::size_t m = k + 1 - matched; m <= k; ++m) {
                search.profane_tokens.push_back(tokens[m]);
            }
        }
    }
    search.node = node;
    search.carried = std::min(tokens.size(), trie.max_spaces);
    tokens.erase(tokens.begin(), tokens.end() - static_cast<std::ptrdiff_t>(search.carried));
}

std::string SwearWordHider::hide_tokens(
    const std::string& text,
    const std::vector<SourceSpan>& spans,
    char censor_char
) {
    std::string censored_text;
    censored_text.reserve(text.size());
    std::size_t copied = 0;
    for (const SourceSpan& span : spans) {
        censored_text.append(text, copied, span.start - copied);
        censored_text.append(span.stars, censor_char);
        copied = span.end;
    }
    censored_text.append(text, copied);
    return censored_text;
}

std::uint32_t SwearWordHider::next_node(const Trie& trie, std::uint32_t node, char32_t ch) {
    if (ch < trie.columns.size()) {
        return trie.nodes[node].ascii[trie.columns[ch]];
    }
    for (;;) {
        std::unordered_map<char32_t, std::uint32_t>::const_iterator child = trie.nodes[node].others.find(ch);
        if (child != trie.nodes[node].others.end()) {
            return child->second;
        }
        if (node == 0) {
            return 0;
        }
        node = trie.nodes[node].fail;
    }
}

std::size_t SwearWordHider::matched_tokens(
    const std::string& text,
    const std::vector<Token>& tokens,
    std::size_t last,
    const Match& match
) {
    std::size_t spaces = match.spaces;
    if (spaces > last) {
        return 0;
    }
    std::size_t length = spaces;
    for (std::size_t k = last - spaces; k <= last; ++k) {
        length += tokens[k].end - tokens[k].start;
        if (tokens[k].separator != '\0') {
            length -= static_cast<std::size_t>(std::count(
                text.begin() + static_cast<std::ptrdiff_t>(tokens[k].start),
                text.begin() + static_cast<std::ptrdiff_t>(tokens[k].end),
                tokens[k].separator
            ));
        }
    }
    return length == match.length ? spaces + 1 : 0;
}
