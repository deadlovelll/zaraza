#pragma once

#include <cstddef>

struct SourceSpan {
    std::size_t start;
    std::size_t end;
    std::size_t stars;

    bool operator==(const SourceSpan&) const = default;
};
