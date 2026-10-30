#pragma once

#include "diacritics_stripper/diacritics_stripper.hpp"

struct utf8proc_property_struct;

// Which normalization step a character needs. Regular ones are only lowercased
enum class CharKind { Ascii, Invalid, Invisible, Space, Compatibility, CaseFolding, Diacritic, Mark, Regular };

class CharClassifier {
public:
    CharKind classify(char32_t code_point);
private:
    DiacriticsStripper diacritics_stripper;

    CharKind classify_special(char32_t code_point, const utf8proc_property_struct* property);
    bool is_space(char32_t code_point, const utf8proc_property_struct* property);
    bool is_invisible(char32_t code_point, const utf8proc_property_struct* property);
};
