#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "source_span/source_span.hpp"
#include "swear_word_hider/swear_word_hider.hpp"

namespace pugi {
class xml_node;
}

class OfficeArchive;
struct OfficeText;
struct TextSegment;

class OfficeCensor {
public:
    std::vector<std::string> text_parts(OfficeArchive& archive);
    bool load(
        OfficeArchive& archive,
        const std::string& name,
        OfficeText& text
    );
    std::string hide(
        OfficeText& text,
        const std::vector<SourceSpan>& spans,
        char censor_char
    );
private:
    static constexpr std::array<std::string_view, 10> text_types = {
        ".main+xml",
        ".header+xml",
        ".footer+xml",
        ".footnotes+xml",
        ".endnotes+xml",
        ".comments+xml",
        ".sharedStrings+xml",
        ".worksheet+xml",
        ".slide+xml",
        ".notesSlide+xml",
    };

    SwearWordHider swear_word_hider;

    bool is_text_type(std::string_view type);
    std::size_t count_chars(std::string_view text);
    std::string_view local_name(pugi::xml_node node);
    bool collect_node(
        pugi::xml_node node,
        std::string& text,
        std::vector<TextSegment>& segments
    );
    void end_paragraph(pugi::xml_node node, std::string& text);
    void collect(
        pugi::xml_node root,
        std::string& text,
        std::vector<TextSegment>& segments
    );
    void hide_segments(
        const std::string& text,
        const std::vector<TextSegment>& segments,
        const std::vector<SourceSpan>& spans,
        char censor_char
    );
};
