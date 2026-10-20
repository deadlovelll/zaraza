#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "swear_word_hider/swear_word_hider.hpp"

std::vector<Token> SwearWordHider::find_profane_tokens(std::vector<Token> tokens, Node* root) {
    std::vector<bool> profane(tokens.size(), false);
    Node* node = root;
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        if (k > 0) {
            node = next_node(node, " ", root);
        }
        const std::string& text = tokens[k].text;
        for (std::size_t i = 0; i < text.size();) {
            std::size_t len = utf8_sizer.utf8_char_len(static_cast<unsigned char>(text[i]));
            node = next_node(node, text.substr(i, len), root);
            i += len;
        }
        for (const std::string& word : node->outputs) {
            std::size_t matched = matched_tokens(tokens, k, word);
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

Node* SwearWordHider::next_node(Node* node, const std::string& ch, Node* root) {
    while (node && !node->children.count(ch)) node = node->fail;
    return node ? node->children[ch].get() : root;
}

std::size_t SwearWordHider::matched_tokens(const std::vector<Token>& tokens, std::size_t last, const std::string& word) {
    std::size_t spaces = static_cast<std::size_t>(std::count(word.begin(), word.end(), ' '));
    if (spaces > last) {
        return 0;
    }
    std::size_t length = spaces;
    for (std::size_t k = last - spaces; k <= last; ++k) {
        length += tokens[k].text.size();
    }
    return length == word.size() ? spaces + 1 : 0;
}
