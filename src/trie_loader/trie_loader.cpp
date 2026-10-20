#include "trie_loader/trie_loader.hpp"

#include <algorithm>
#include <queue>
#include <utility>

#include <utf8proc.h>

std::vector<Node> TrieLoader::get_trie() {
    return std::move(nodes);
}

// Loads all profane words from set into trie
void TrieLoader::add_words(const std::unordered_set<std::string>& words) {
    for (const std::string& word: words) {
        if (word.size() <= 1) continue;
        std::uint32_t node = 0;

        for (size_t i = 0; i < word.size();) {
            utf8proc_int32_t code_point;
            utf8proc_ssize_t len = utf8proc_iterate(
                reinterpret_cast<const utf8proc_uint8_t*>(word.data()) + i,
                static_cast<utf8proc_ssize_t>(word.size() - i),
                &code_point
            );
            char32_t ch = static_cast<char32_t>(code_point);
            std::uint32_t next = child(node, ch);
            // Create new node if it doesn't exist
            if (next == 0) {
                next = static_cast<std::uint32_t>(nodes.size());
                nodes.emplace_back();
                if (ch < nodes[node].ascii.size()) {
                    nodes[node].ascii[ch] = next;
                } else {
                    nodes[node].others[ch] = next;
                }
            }
            // Descend to child
            node = next;
            i += static_cast<size_t>(len);
        }
        // Mark the end of word
        nodes[node].matches.push_back({word.size(), static_cast<std::size_t>(std::count(word.begin(), word.end(), ' '))});
    }
}

void TrieLoader::build() {
    // Bfs queue
    std::queue<std::uint32_t> q;
    for (std::uint32_t next : nodes[0].ascii) {
        if (next != 0) {
            q.push(next);
        }
    }
    for (const std::pair<const char32_t, std::uint32_t>& edge : nodes[0].others) {
        q.push(edge.second);
    }
    while (!q.empty()) {
        std::uint32_t curr = q.front();
        q.pop();
        for (char32_t ch = 0; ch < nodes[curr].ascii.size(); ++ch) {
            std::uint32_t next = nodes[curr].ascii[ch];
            std::uint32_t fail_next = nodes[nodes[curr].fail].ascii[ch];
            if (next == 0) {
                nodes[curr].ascii[ch] = fail_next;
            } else {
                link(next, fail_next);
                q.push(next);
            }
        }
        for (const std::pair<const char32_t, std::uint32_t>& edge : nodes[curr].others) {
            link(edge.second, fail_child(nodes[curr].fail, edge.first));
            q.push(edge.second);
        }
    }
}

std::uint32_t TrieLoader::child(std::uint32_t node, char32_t ch) {
    if (ch < nodes[node].ascii.size()) {
        return nodes[node].ascii[ch];
    }
    std::unordered_map<char32_t, std::uint32_t>::const_iterator next = nodes[node].others.find(ch);
    return next == nodes[node].others.end() ? 0 : next->second;
}

std::uint32_t TrieLoader::fail_child(std::uint32_t node, char32_t ch) {
    for (;;) {
        std::unordered_map<char32_t, std::uint32_t>::const_iterator next = nodes[node].others.find(ch);
        if (next != nodes[node].others.end()) {
            return next->second;
        }
        if (node == 0) {
            return 0;
        }
        node = nodes[node].fail;
    }
}

void TrieLoader::link(std::uint32_t node, std::uint32_t fail) {
    nodes[node].fail = fail;
    // Inherit outputs from fail node
    nodes[node].matches.insert(nodes[node].matches.end(), nodes[fail].matches.begin(), nodes[fail].matches.end());
}
