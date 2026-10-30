#include "char_classifier/char_classifier.hpp"

#include <utf8proc.h>

// One property lookup per character
CharKind CharClassifier::classify(char32_t code_point) {
    const utf8proc_property_t* property = utf8proc_get_property(static_cast<utf8proc_int32_t>(code_point));
    if (
        is_invisible(code_point, property)
        // has a decomposition: Ｆ -> F, é -> e + U+0301
        || property->decomp_seqindex != UINT16_MAX
        // fold differs from lowercase: ß -> ss
        || property->casefold_seqindex != property->lowercase_seqindex
        // standalone accent: U+0301
        || property->category == UTF8PROC_CATEGORY_MN
        // standalone enclosing mark: U+20DD
        || property->category == UTF8PROC_CATEGORY_ME
        || property->category == UTF8PROC_CATEGORY_MC
        || is_space(code_point, property)) [[unlikely]]
    {
        return classify_special(code_point, property);
    }
    return CharKind::Regular;
}

CharKind CharClassifier::classify_special(char32_t code_point, const utf8proc_property_t* property) {
    if (is_space(code_point, property)) {
        return CharKind::Space;
    }
    if (is_invisible(code_point, property)) {
        return CharKind::Invisible;
    }
    // standalone accent: Zalgo, f + U+0336
    if (
        property->category == UTF8PROC_CATEGORY_MN || property->category == UTF8PROC_CATEGORY_MC
        || property->category == UTF8PROC_CATEGORY_ME)
    {
        return diacritics_stripper.is_diacritic_mark(code_point) ? CharKind::Diacritic : CharKind::Mark;
    }
    // compatibility decomposition: Ｆ -> F, ﬁ -> fi
    if (property->decomp_type != 0) {
        return CharKind::Compatibility;
    }
    // accented letter: é -> e + U+0301
    if (property->decomp_seqindex != UINT16_MAX) {
        return CharKind::Diacritic;
    }
    // fold differs from lowercase: ß -> ss
    if (property->casefold_seqindex != property->lowercase_seqindex) {
        return CharKind::CaseFolding;
    }
    return CharKind::Regular;
}

bool CharClassifier::is_space(char32_t code_point, const utf8proc_property_t* property) {
    const char32_t next_line = 0x85;
    return property->category == UTF8PROC_CATEGORY_ZS || property->category == UTF8PROC_CATEGORY_ZL
        || property->category == UTF8PROC_CATEGORY_ZP || code_point == U'⠀' || code_point == next_line;
}

// ZWSP and soft hyphen, other format and control characters, unassigned ignorables (U+2065, U+FFF0),
// and a modifier letter inside a word: dipsʻhit
bool CharClassifier::is_invisible(char32_t code_point, const utf8proc_property_t* property) {
    return property->ignorable || property->category == UTF8PROC_CATEGORY_CF || property->category == UTF8PROC_CATEGORY_CC
        || (code_point >= 0x2060 && code_point <= 0x206F) || (code_point >= 0xFFF0 && code_point <= 0xFFF8)
        || (code_point >= 0xE0000 && code_point <= 0xE0FFF)
        || (
            code_point >= 0x2B0 && code_point <= 0x2FF && property->decomp_type == 0
            && (property->category == UTF8PROC_CATEGORY_LM || property->category == UTF8PROC_CATEGORY_SK)
        );
}
