#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "swear_word_hider/swear_word_hider.hpp"

#include <utf8proc.h>

std::vector<Token> SwearWordHider::find_profane_tokens(const std::string& text, std::vector<Token> tokens, Node* root) {
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    std::vector<bool> profane(tokens.size(), false);
    Node* node = root;
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        if (k > 0) {
            node = next_node(node, U' ', root);
        }
        for (std::size_t i = tokens[k].start; i < tokens[k].end;) {
            if (text[i] == tokens[k].separator) {
                ++i;
                continue;
            }
            utf8proc_int32_t code_point = bytes[i];
            utf8proc_ssize_t len = 1;
            if (code_point >= 0x80) {
                len = utf8proc_iterate(bytes + i, static_cast<utf8proc_ssize_t>(tokens[k].end - i), &code_point);
            }
            node = next_node(node, static_cast<char32_t>(code_point), root);
            i += static_cast<std::size_t>(len);
        }
        for (const std::string& word : node->outputs) {
            std::size_t matched = matched_tokens(text, tokens, k, word);
            for (std::size_t m = k + 1 - matched; m <= k; ++m) {
                profane[m] = true;
            }
        }
    }
    std::vector<Token> profane_tokens;
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        if (profane[k]) {
            profane_tokens.push_back(std::move(tokens[k]));
        }
    }
    return profane_tokens;
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

Node* SwearWordHider::next_node(Node* node, char32_t ch, Node* root) {
    for (; node != nullptr; node = node->fail) {
        std::unordered_map<char32_t, std::unique_ptr<Node>>::const_iterator child = node->children.find(ch);
        if (child != node->children.end()) {
            return child->second.get();
        }
    }
    return root;
}

std::size_t SwearWordHider::matched_tokens(
    const std::string& text,
    const std::vector<Token>& tokens,
    std::size_t last,
    const std::string& word
) {
    std::size_t spaces = static_cast<std::size_t>(std::count(word.begin(), word.end(), ' '));
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
    return length == word.size() ? spaces + 1 : 0;
}
