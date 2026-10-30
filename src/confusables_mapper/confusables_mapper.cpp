#include "confusables_mapper/confusables_mapper.hpp"
#include "confusables_mapper/confusables.hpp"
#include "confusables_mapper/stylized_letters.hpp"

#include <algorithm>

#include <utf8proc.h>

// Lookalikes below U+10000 are put in a table once, at program start: one lookup per letter
const ConfusablesMapper::Table ConfusablesMapper::table = build_table();

char ConfusablesMapper::to_latin(char32_t code_point) {
    if (code_point >= table.size()) [[unlikely]] {
        return to_latin_beyond_table(code_point);
    }
    return table[code_point];
}

// Rare letters beyond U+FFFF: binary search in the sorted list
char ConfusablesMapper::to_latin_beyond_table(char32_t code_point) {
    std::array<Confusable, confusables.size()>::const_iterator found = std::ranges::lower_bound(
        confusables, code_point, {}, &Confusable::code_point
    );
    if (found != confusables.end() && found->code_point == code_point) {
        return found->latin;
    }
    return 0;
}

// Latin letter for every lookalike below U+10000, 0 for the rest
ConfusablesMapper::Table ConfusablesMapper::build_table() {
    Table built{};
    for (const Confusable& confusable : confusables) {
        if (confusable.code_point < built.size()) {
            built[confusable.code_point] = confusable.latin;
        }
    }
    for (const Confusable& letter : stylized_letters) {
        built[letter.code_point] = letter.latin;
    }
    for (const Confusable& confusable : confusables) {
        char32_t lowered = static_cast<char32_t>(utf8proc_tolower(static_cast<utf8proc_int32_t>(confusable.code_point)));
        if (lowered < built.size() && built[lowered] == 0) {
            built[lowered] = confusable.latin;
        }
    }
    return built;
}
