#include <algorithm>
#include <bit>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
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
    std::size_t next = search.carried;
    for (std::size_t block = search.carried; block < tokens.size(); block += 64) {
        std::uint64_t keep = trie.filter.may_match(text, tokens, block);
        while (keep != 0) {
            std::size_t k = block + static_cast<std::size_t>(std::countr_zero(keep));
            keep &= keep - 1;
            // A skipped token can't be inside a match
            if (k != next) {
                node = 0;
            }
            next = k + 1;
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
            for (
                const Match& match : 
                reinterpret_cast<const Node*>(reinterpret_cast<const char*>(trie.nodes.data()) + node)->matches
            ) {
                std::size_t matched = matched_tokens(text, tokens, k, match);
                if (matched == 0) {
                    continue;
                }
                while (
                    !search.profane_tokens.empty() 
                    && search.profane_tokens.back().start >= 
                    search.dropped + tokens[k + 1 - matched].start
                ) {
                    search.profane_tokens.pop_back();
                }
                for (std::size_t m = k + 1 - matched; m <= k; ++m) {
                    search.profane_tokens.push_back({search.dropped + tokens[m].start, search.dropped + tokens[m].end, tokens[m].separator});
                }
            }
        }
    }
    if (next != tokens.size()) {
        node = 0;
    }
    search.node = node;
    search.carried = std::min(tokens.size(), trie.max_spaces);
    tokens.erase(tokens.begin(), tokens.end() - static_cast<std::ptrdiff_t>(search.carried));
}

std::string SwearWordHider::hide_tokens(
    std::string_view text,
    const std::vector<SourceSpan>& spans
) {
    std::string censored_text;
    censored_text.reserve(text.size());
    std::size_t copied = 0;
    for (const SourceSpan& span : spans) {
        censored_text.append(text, copied, span.start - copied);
        censored_text.append(span.stars, '*');
        copied = span.end;
    }
    censored_text.append(text, copied);
    return censored_text;
}

std::size_t SwearWordHider::hide_tokens_inplace(
    std::span<char> text,
    const std::vector<SourceSpan>& spans
) {
    std::size_t written = 0;
    std::size_t copied = 0;
    for (const SourceSpan& span : spans) {
        if (written != copied) {
            std::memmove(text.data() + written, text.data() + copied, span.start - copied);
        }
        written += span.start - copied;
        // Never write over input that is not copied yet
        std::size_t stars = std::min(span.stars, span.end - written);
        std::memset(text.data() + written, '*', stars);
        written += stars;
        copied = span.end;
    }
    if (written != copied) {
        std::memmove(text.data() + written, text.data() + copied, text.size() - copied);
    }
    return written + text.size() - copied;
}

std::uint32_t SwearWordHider::next_node(const Trie& trie, std::uint32_t node, char32_t ch) {
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
