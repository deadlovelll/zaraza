#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "char_classifier/char_classifier.hpp"
#include "diacritics_stripper/diacritics_stripper.hpp"
#include "output_buffer/output_buffer.hpp"
#include "source_char/source_char.hpp"
#include "source_span/source_span.hpp"
#include "token/token.hpp"

// Unambiguous per-character canonicalization done in a single pass over UTF-8 text:
// removes invisible characters, replaces compatibility characters (fullwidth, mathematical,
// circled, ligatures...) with ordinary ones, applies full Unicode case folding (ß -> ss),
// strips diacritics (é -> e) and replaces lookalikes of Latin letters with them (Cyrillic с -> c).
class UnicodeNormalizer {
public:
    [[nodiscard]] std::string normalize(std::string_view text);
    std::vector<SourceSpan> map_to_source(std::string_view text, std::vector<Token> tokens);
private:
    CharClassifier char_classifier;
    DiacriticsStripper diacritics_stripper;

    template <bool with_offsets>
    std::string normalize_text(
        std::string_view source,
        const std::vector<Token>* tokens,
        std::vector<SourceSpan>* spans
    );

    SourceChar read_char(std::string_view text, std::ptrdiff_t pos);
    CharKind classify_char(SourceChar source_char);
    void recompose_separate_letter(std::string_view text, SourceChar& source_char, CharKind& kind);
    OutputBuffer append_regional_indicator(OutputBuffer out, SourceChar source_char, std::string_view text);
    OutputBuffer append_source(OutputBuffer out, SourceChar source_char, std::string_view text);
    OutputBuffer append_compatibility_form(OutputBuffer out, char32_t code_point);
    OutputBuffer append_case_folded(OutputBuffer out, char32_t code_point);
    OutputBuffer append_without_diacritics(OutputBuffer out, char32_t code_point);
    OutputBuffer append_code_point(OutputBuffer out, char32_t code_point);
    bool is_two_byte_char(std::string_view text, std::ptrdiff_t pos);
    bool is_regional_indicator(char32_t code_point);
    bool is_regional_indicator_at(std::string_view text, std::ptrdiff_t pos);
    char32_t last_code_point(std::string_view text);
    char32_t lower(char32_t code_point);
    char lower_ascii(char c);
};
