#pragma once

#include <cstddef>

#include "pugixml.hpp"

struct TextSegment {
    pugi::xml_node node;
    std::size_t start;
    std::size_t end;
};
