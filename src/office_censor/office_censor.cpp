#include "office_censor/office_censor.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "office_archive/office_archive.hpp"
#include "office_text/office_text.hpp"

std::size_t OfficeCensor::count_chars(std::string_view text) {
    std::size_t chars = 0;
    for (char byte : text) {
        // 10xxxxxx is a utf8 continuation byte
        if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80) {
            ++chars;
        }
    }
    return chars;
}

std::string_view OfficeCensor::local_name(pugi::xml_node node) {
    std::string_view name = node.name();
    std::size_t colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

// collects text of the node into text and segments,
// returns true if the node is a container and its children must be visited
bool OfficeCensor::collect_node(
    pugi::xml_node node,
    std::string& text,
    std::vector<TextSegment>& segments
) {
    if (node.type() != pugi::node_element) {
        return false;
    }
    std::string_view name = local_name(node);
    // this is text according to OXML specification
    if (name == "t") {
        std::string_view value = node.text().get();
        segments.push_back({node, text.size(), text.size() + value.size()});
        text += value;
        return false;
    }
    if (
        // tabulation
        name == "tab"
        // line or page break
        || name == "br"
        // carriage return
        || name == "cr"
    ) {
        text += ' ';
        return false;
    }
    return true;
}

// adds \n after a paragraph so paragraphs do not glue together
void OfficeCensor::end_paragraph(pugi::xml_node node, std::string& text) {
    std::string_view name = local_name(node);
    if (
        // word and powerpoint paragraph
        name == "p"
        // excel shared string
        || name == "si"
        // excel inline string
        || name == "is"
    ) {
        text += '\n';
    }
}

// every office file like doc excel powerpoint etc is just an archive with xmls
std::vector<std::string> OfficeCensor::text_parts(OfficeArchive& archive) {
    std::string content_types;
    // read the head of the archive
    if (!archive.read("[Content_Types].xml", content_types)) {
        throw std::invalid_argument("censor_file: only DOCX, XLSX and PPTX archives are supported");
    }
    // parse text xml into tree
    pugi::xml_document document;
    if (!document.load_buffer_inplace(content_types.data(), content_types.size())) {
        throw std::invalid_argument("censor_file: broken or unsupported ZIP archive");
    }
    std::vector<std::string> parts;
    // archive parts listed in [Content_Types].xml
    for (pugi::xml_node part : document.document_element().children("Override")) {
        std::string_view type = part.attribute("ContentType").value();
        std::string_view name = part.attribute("PartName").value();
        if (
            // based on OXML specification name of the part should start with /
            // otherwise its not a valid one
            // ECMA-376 Part 2, 6.2.2.2: part_name = 1*( "/" isegment-nz )
            // https://ecma-international.org/publications-and-standards/standards/ecma-376/
            name.starts_with('/') 
            // check if this part is containing text of the doc
            && is_text_type(type)
        ) {
            parts.emplace_back(name.substr(1));
        }
    }
    return parts;
}

bool OfficeCensor::is_text_type(std::string_view type) {
    for (std::string_view text_type : text_types) {
        if (type.ends_with(text_type)) {
            return true;
        }
    }
    return false;
}

bool OfficeCensor::load(
    OfficeArchive& archive,
    const std::string& name,
    OfficeText& text
) {
    if (!archive.read(name, text.xml)) {
        return false;
    }
    // this is fast-path check if the current xml file really contains something
    // by OXML specification text contains in the <t> blocks
    if (
        text.xml.find(":t>") == std::string::npos
        && text.xml.find(":t ") == std::string::npos
        && text.xml.find("<t>") == std::string::npos
        && text.xml.find("<t ") == std::string::npos
    ) {
        return false;
    }
    // keep declaration, comments, processing instructions and doctype
    unsigned int flags = pugi::parse_full;
    // keep whitespace-only text, e.g. a space between words in <w:t>
    flags |= pugi::parse_ws_pcdata;
    // keep \r\n line endings as is
    flags &= ~pugi::parse_eol;
    // keep tabs and newlines inside attribute values as is
    flags &= ~pugi::parse_wconv_attribute;

    // load text xml into tree
    if (!text.document.load_buffer_inplace(text.xml.data(), text.xml.size(), flags, pugi::encoding_utf8)) {
        throw std::invalid_argument("censor_file: broken XML in the archive");
    }
    collect(text.document, text.text, text.segments);
    return true;
}

std::string OfficeCensor::hide(
    OfficeText& text,
    const std::vector<SourceSpan>& spans
) {
    hide_segments(text.text, text.segments, spans);
    // write xml as is, without added indentation and line breaks
    unsigned int format = pugi::format_raw;
    // do not add <?xml?> declaration if the part had none
    format |= pugi::format_no_declaration;
    // office xml parts are always utf-8
    pugi::xml_encoding encoding = pugi::encoding_utf8;
    std::ostringstream saved;
    text.document.save(saved, "", format, encoding);
    return std::move(saved).str();
}

void OfficeCensor::collect(
    pugi::xml_node root,
    std::string& text,
    std::vector<TextSegment>& segments
) {
    pugi::xml_node node = root.first_child();
    while (node) {
        bool container = collect_node(node, text, segments);
        if (container && node.first_child()) {
            node = node.first_child();
            continue;
        }
        if (container) {
            end_paragraph(node, text);
        }
        while (!node.next_sibling() && node.parent() != root) {
            node = node.parent();
            end_paragraph(node, text);
        }
        node = node.next_sibling();
    }
}

void OfficeCensor::hide_segments(
    const std::string& text,
    const std::vector<TextSegment>& segments,
    const std::vector<SourceSpan>& spans
) {
    std::size_t next = 0;
    std::size_t stars = spans[0].stars;
    // segments is the words we found in the xml document
    for (const TextSegment& segment : segments) {
        std::vector<SourceSpan> clipped;
        // spans is the profane words we found
        while (next < spans.size() && spans[next].start < segment.end) {
            // this code block decides which part of the match is in this segment and how many stars it gets
            std::size_t from = std::max(spans[next].start, segment.start);
            std::size_t to = std::min(spans[next].end, segment.end);
            if (from < to) {
                std::string_view part(text.data() + from, to - from);
                std::size_t chars = count_chars(part);
                std::size_t here = to == spans[next].end ? stars : std::min(stars, chars);
                clipped.push_back({from - segment.start, to - segment.start, here});
                stars -= here;
            }
            if (spans[next].end > segment.end) {
                break;
            }
            ++next;
            stars = next < spans.size() ? spans[next].stars : 0;
        }
        if (!clipped.empty()) {
            std::string_view value(text.data() + segment.start, segment.end - segment.start);
            segment.node.text().set(swear_word_hider.hide_tokens(value, clipped).c_str());
        }
    }
}
