#pragma once

#include <string>
#include <vector>

#include "pugixml.hpp"
#include "text_segment/text_segment.hpp"

struct OfficeText {
    std::string xml;
    pugi::xml_document document;
    std::string text;
    std::vector<TextSegment> segments;
};
