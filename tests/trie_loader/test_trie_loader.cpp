#include <gtest/gtest.h>

#include <string>
#include <unordered_set>

#include "trie_loader/trie_loader.hpp"

TEST(TrieLoaderTest, ReservesExactNodeCount) {
    std::unordered_set<std::string> words = {"fuck", "fucker", "fun", "son of a bitch", "блять", "бля", "ж", "x"};
    TrieLoader trie_loader(words);
    Trie trie = trie_loader.get_trie();
    EXPECT_EQ(trie.nodes.size(), 28u);
    EXPECT_EQ(trie.nodes.capacity(), trie.nodes.size());
}
