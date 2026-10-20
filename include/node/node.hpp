#pragma once

#include <unordered_map>
#include <vector>

struct Node {
    std::unordered_map<char32_t, std::unique_ptr<Node>> children;
    Node* fail = nullptr;
    std::vector<std::string> outputs;
    size_t depth;
};
