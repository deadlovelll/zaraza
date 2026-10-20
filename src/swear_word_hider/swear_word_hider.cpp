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
        node = next_node(trie.nodes, node, U' ');
        const Token token = tokens[k];
        for (std::size_t i = token.start; i < token.end;) {
            if (bytes[i] == static_cast<utf8proc_uint8_t>(token.separator)) {
                ++i;
                continue;
            }
            utf8proc_int32_t code_point = bytes[i];
            utf8proc_ssize_t len = 1;
            if (code_point >= 0x80) {
                len = utf8proc_iterate(bytes + i, static_cast<utf8proc_ssize_t>(token.end - i), &code_point);
            }
            node = next_node(trie.nodes, node, static_cast<char32_t>(code_point));
            i += static_cast<std::size_t>(len);
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

std::vector<std::size_t> SwearWordHider::token_positions(const std::vector<Token>& tokens) {
    std::size_t count = 0;
    for (const Token& token : tokens) {
        count += token.end - token.start + 1;
    }
    std::vector<std::size_t> positions;
    positions.reserve(count);
    for (const Token& token : tokens) {
        for (std::size_t position = token.start; position <= token.end; ++position) {
            positions.push_back(position);
        }
    }
    return positions;
}

std::string SwearWordHider::hide_tokens(
    const std::string& text,
    const std::vector<Token>& tokens,
    const std::vector<std::size_t>& source_offsets,
    char censor_char
) {
    std::string censored_text;
    std::size_t copied = 0;
    std::size_t first = 0;
    for (const Token& token : tokens) {
        std::size_t last = first + token.end - token.start;
        censored_text.append(text, copied, source_offsets[first] - copied);
        for (std::size_t i = first; i < last; ++i) {
            if (i == first || source_offsets[i] != source_offsets[i - 1]) {
                censored_text.push_back(censor_char);
            }
        }
        copied = source_offsets[last];
        first = last + 1;
    }
    censored_text.append(text, copied);
    return censored_text;
}

std::uint32_t SwearWordHider::next_node(const std::vector<Node>& trie, std::uint32_t node, char32_t ch) {
    if (ch < trie[node].ascii.size()) {
        return trie[node].ascii[ch];
    }
    for (;;) {
        std::unordered_map<char32_t, std::uint32_t>::const_iterator child = trie[node].others.find(ch);
        if (child != trie[node].others.end()) {
            return child->second;
        }
        if (node == 0) {
            return 0;
        }
        node = trie[node].fail;
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
