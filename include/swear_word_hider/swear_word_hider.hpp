#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "node/node.hpp"
#include "profane_search/profane_search.hpp"
#include "source_span/source_span.hpp"
#include "tokenizer/tokenizer.hpp"

class SwearWordHider {
public:
    void find_profane_tokens(
        const std::string& text,
        std::vector<Token>& tokens,
        const Trie& trie,
        ProfaneSearch& search
    );
    std::string hide_tokens(
        std::string_view text,
        const std::vector<SourceSpan>& spans
    );
    std::size_t hide_tokens_inplace(
        std::span<char> text,
        const std::vector<SourceSpan>& spans
    );
private:
    std::uint32_t next_node(const Trie& trie, std::uint32_t node, char32_t ch);
    std::size_t matched_tokens(
        const std::string& text,
        const std::vector<Token>& tokens,
        std::size_t last,
        const Match& match
    );
};