#include "unicode_normalizer/unicode_normalizer.hpp"
#include "confusables_mapper/confusables_mapper.hpp"

#include <cstring>

#include <utf8proc.h>

std::string UnicodeNormalizer::normalize(std::string_view text) {
    return normalize_text<false>(text, nullptr, nullptr);
}

std::size_t UnicodeNormalizer::normalize_batch(
    std::string_view text,
    std::size_t from,
    std::string& normalized
) {
    const std::size_t batch_size = 65536;
    const std::size_t block_size = 64;
    const std::size_t max_stride = 4096;
    std::size_t to = std::min(text.size(), from + batch_size);
    while (to < text.size() && !is_batch_start(text, to)) {
        ++to;
    }
    std::size_t done = from;
    std::size_t stride = block_size;
    for (std::size_t pos = from; done < to;) {
        char lowered[block_size];
        bool clean = pos + block_size <= to && lower_ascii_block(text.data() + pos, lowered);
        if (!clean && pos + block_size <= to) {
            // Back off on non-ASCII text
            pos += stride;
            stride = std::min(2 * stride, max_stride);
            continue;
        }
        std::size_t end = clean ? pos : to;
        if (done < end) {
            // A mark after a lowered block needs the letter before it
            if (done > from && !is_batch_start(text, done)) {
                normalized.pop_back();
                --done;
            }
            std::string batch = normalize(text.substr(done, end - done));
            if (normalized.empty()) {
                normalized.swap(batch);
            } else {
                normalized += batch;
            }
            done = end;
        }
        if (clean) {
            stride = block_size;
            normalized.append(lowered, block_size);
            pos += block_size;
            done = pos;
        }
    }
    return to;
}

std::vector<SourceSpan> UnicodeNormalizer::map_to_source(
    std::string_view text,
    std::vector<Token> tokens,
    const std::vector<BatchStart>& batch_starts
) {
    std::vector<SourceSpan> spans;
    if (batch_starts.empty()) {
        normalize_text<true>(text, &tokens, &spans);
        return spans;
    }
    spans.reserve(tokens.size());
    std::vector<Token> batch_tokens;
    BatchStart start = {0, 0};
    std::size_t next = 0;
    std::vector<Token>::iterator first = tokens.begin();
    while (first != tokens.end()) {
        while (next < batch_starts.size() && batch_starts[next].normalized <= first->start) {
            start = batch_starts[next];
            ++next;
        }
        std::size_t batch_end = next < batch_starts.size() ? batch_starts[next].normalized : SIZE_MAX;
        std::vector<Token>::iterator last = std::ranges::lower_bound(first, tokens.end(), batch_end, {}, &Token::start);
        batch_tokens.assign(first, last);
        for (Token& token : batch_tokens) {
            token.start -= start.normalized;
            token.end -= start.normalized;
        }
        std::size_t mapped = spans.size();
        normalize_text<true>(text.substr(start.source), &batch_tokens, &spans);
        for (std::size_t i = mapped; i < spans.size(); ++i) {
            spans[i].start += start.source;
            spans[i].end += start.source;
        }
        first = last;
    }
    return spans;
}

template <bool with_offsets>
std::string UnicodeNormalizer::normalize_text(
    std::string_view source,
    const std::vector<Token>* tokens,
    std::vector<SourceSpan>* spans
) {
    const std::size_t offsets_window = 4096;
    const std::size_t max_char_bytes = 4;
    std::string normalized;
    OutputBuffer out(normalized, with_offsets ? std::min(source.size(), offsets_window) : source.size());
    std::size_t dropped = 0;
    std::size_t limit = 0;
    std::size_t before = 0;
    std::size_t produced = 0;
    std::size_t chars_before = 0;
    std::size_t event = 0;
    std::ptrdiff_t split_char = -1;
    if constexpr (with_offsets) {
        spans->reserve(tokens->size());
    }
    const std::ptrdiff_t size = static_cast<std::ptrdiff_t>(source.size());
    const std::uint8_t* fast = fast_outputs().data();
    std::ptrdiff_t pos = 0;
    while (pos < size) {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(source.data());
        const std::size_t room = std::min(limit, offsets_window / 2);
        while (pos < size) {
            unsigned char lead = bytes[pos];
            if (lead < 0x80) {
                if constexpr (with_offsets) {
                    if (out.size() >= room) {
                        break;
                    }
                    ++produced;
                }
                out.push_back(lower_ascii(static_cast<char>(lead)));
                ++pos;
                continue;
            }
            char32_t code_point = 0;
            std::ptrdiff_t len = 0;
            if (lead >= 0xC2 && lead < 0xE0 && pos + 1 < size && (bytes[pos + 1] & 0xC0) == 0x80) {
                code_point = ((lead & 0x1Fu) << 6) | (bytes[pos + 1] & 0x3Fu);
                len = 2;
            } else if (
                lead >= 0xE0 && lead < 0xF0 && pos + 2 < size
                && (bytes[pos + 1] & 0xC0) == 0x80 && (bytes[pos + 2] & 0xC0) == 0x80
            ) {
                code_point = ((lead & 0x0Fu) << 12) | ((bytes[pos + 1] & 0x3Fu) << 6) | (bytes[pos + 2] & 0x3Fu);
                len = code_point >= 0x800 ? 3 : 0;
            }
            if (len == 0 || fast[code_point] == 0) {
                break;
            }
            if constexpr (with_offsets) {
                if (out.size() + (fast[code_point] == 1 ? static_cast<std::size_t>(len) : 1) > room) {
                    break;
                }
                ++produced;
            }
            if (fast[code_point] == 1) {
                out.append(source.data() + pos, static_cast<std::size_t>(len));
            } else {
                out.push_back(static_cast<char>(fast[code_point]));
            }
            pos += len;
        }
        if (pos == size) {
            break;
        }
        if constexpr (with_offsets) {
            before = out.size();
        }
        SourceChar source_char = read_char(source, pos);
        CharKind kind = classify_char(source_char);
        recompose_separate_letter(source, source_char, kind);
        switch (kind) {
            case CharKind::Ascii:
                out.push_back(lower_ascii(source[static_cast<std::size_t>(source_char.start)]));
                break;
            case CharKind::Invalid:
                out.push_back(source[static_cast<std::size_t>(source_char.start)]);
                break;
            case CharKind::Invisible:
                break;
            case CharKind::Space:
                out.push_back(' ');
                break;
            case CharKind::CaseFolding:
                out = append_case_folded(out, source_char.code_point);
                break;
            case CharKind::Compatibility:
                out = append_compatibility_form(out, source_char.code_point);
                break;
            case CharKind::Diacritic:
                out = append_without_diacritics(out, source_char.code_point);
                break;
            case CharKind::Mark:
                if (diacritics_stripper.is_same_script(last_code_point(out.view()), source_char.code_point)) {
                    out = append_source(out, source_char, source);
                }
                break;
            case CharKind::Regular: {
                // Checked before lowering: some letters look Latin only in uppercase, Cyrillic В -> b
                char latin = ConfusablesMapper::to_latin(source_char.code_point);
                if (latin != 0) {
                    out.push_back(latin);
                    break;
                }
                if (is_regional_indicator(source_char.code_point)) [[unlikely]] {
                    out = append_regional_indicator(out, source_char, source);
                    break;
                }
                char32_t lowered = lower(source_char.code_point);
                if (lowered == source_char.code_point) {
                    out = append_source(out, source_char, source);
                } else {
                    out = append_code_point(out, lowered);
                }
                break;
            }
        }
        if constexpr (with_offsets) {
            produced += out.size() > before ? 1 : 0;
            if (out.size() > limit) {
                for (; event < 2 * tokens->size(); ++event) {
                    const Token& token = (*tokens)[event / 2];
                    limit = (event % 2 == 0 ? token.start : token.end) - dropped;
                    if (out.size() <= limit) {
                        break;
                    }
                    if (event % 2 == 0) {
                        // A token that starts where the previous one ended mid-character shares its span
                        if (source_char.start != split_char) {
                            spans->push_back({static_cast<std::size_t>(source_char.start), 0, 0});
                            chars_before = produced - 1;
                        }
                    } else if (before < limit) {
                        // The token ends inside this character's expansion (⒈ -> 1.), so the whole character is hidden
                        spans->back().end = static_cast<std::size_t>(source_char.end);
                        spans->back().stars = produced - chars_before;
                        split_char = source_char.start;
                    } else {
                        spans->back().end = static_cast<std::size_t>(source_char.start);
                        spans->back().stars = produced - 1 - chars_before;
                    }
                }
                if (event == 2 * tokens->size()) {
                    break;
                }
            }
            if (out.size() > offsets_window / 2) [[unlikely]] {
                dropped += out.size() - max_char_bytes;
                limit -= out.size() - max_char_bytes;
                out.keep_last(max_char_bytes);
            }
        }
        pos = source_char.end;
    }
    if constexpr (with_offsets) {
        if (event % 2 == 1) {
            spans->back().end = source.size();
            spans->back().stars = produced - chars_before;
        }
    }
    out.finish();
    return normalized;
}

bool UnicodeNormalizer::is_batch_start(std::string_view text, std::size_t pos) {
    if (static_cast<unsigned char>(text[pos]) < 0x80) {
        return true;
    }
    SourceChar source_char = read_char(text, static_cast<std::ptrdiff_t>(pos));
    return classify_char(source_char) == CharKind::Regular && !is_regional_indicator(source_char.code_point);
}

SourceChar UnicodeNormalizer::read_char(std::string_view text, std::ptrdiff_t pos) {
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    if (bytes[pos] < 0x80) {
        return {bytes[pos], pos, pos + 1};
    }
    if (is_two_byte_char(text, pos)) {
        return {static_cast<char32_t>(((bytes[pos] & 0x1F) << 6) | (bytes[pos + 1] & 0x3F)), pos, pos + 2};
    }
    utf8proc_int32_t code_point;
    utf8proc_ssize_t len = utf8proc_iterate(bytes + pos, static_cast<utf8proc_ssize_t>(text.size()) - pos, &code_point);
    return {static_cast<char32_t>(code_point), pos, pos + (len > 0 ? len : 1)};
}

CharKind UnicodeNormalizer::classify_char(SourceChar source_char) {
    if (source_char.code_point < 0x80) {
        return CharKind::Ascii;
    }
    if (source_char.code_point > 0x10FFFF) {
        return CharKind::Invalid;
    }
    return char_classifier.classify(source_char.code_point);
}

void UnicodeNormalizer::recompose_separate_letter(std::string_view text, SourceChar& source_char, CharKind& kind) {
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    const utf8proc_ssize_t size = static_cast<utf8proc_ssize_t>(text.size());
    const utf8proc_int32_t above = 230;
    if (
        kind == CharKind::Regular && source_char.end < size && (bytes[source_char.end] & 0xFE) == 0xCC
        && diacritics_stripper.is_separate_letter_base(source_char.code_point)) [[unlikely]]
    {
        // Marks of other classes may stand between, like in NFC: и + U+0323 + U+0306 is still й
        for (utf8proc_ssize_t pos = source_char.end; pos < size;) {
            utf8proc_int32_t mark;
            utf8proc_ssize_t mark_len = utf8proc_iterate(bytes + pos, size - pos, &mark);
            if (mark_len <= 0 || !diacritics_stripper.is_diacritic_mark(static_cast<char32_t>(mark))) {
                return;
            }
            char32_t separate_letter = diacritics_stripper.compose_separate_letter(source_char.code_point, static_cast<char32_t>(mark));
            if (separate_letter != 0) {
                source_char.code_point = separate_letter;
                source_char.end = pos + mark_len;
                kind = CharKind::Diacritic;
                return;
            }
            // The breve and the diaeresis are marks above: a mark above or a class 0 one before them blocks them
            utf8proc_int32_t combining_class = utf8proc_get_property(mark)->combining_class;
            if (combining_class == 0 || combining_class == above) {
                return;
            }
            pos += mark_len;
        }
    }
}

OutputBuffer UnicodeNormalizer::append_regional_indicator(
    OutputBuffer out,
    SourceChar source_char,
    std::string_view text
) {
    if (is_regional_indicator_at(text, source_char.start - 4) || is_regional_indicator_at(text, source_char.end)) {
        out = append_source(out, source_char, text);
    } else {
        out.push_back(static_cast<char>('a' + (source_char.code_point - U'\U0001F1E6')));
    }
    return out;
}

OutputBuffer UnicodeNormalizer::append_source(
    OutputBuffer out,
    SourceChar source_char,
    std::string_view text
) {
    out.append(text.data() + source_char.start, static_cast<std::size_t>(source_char.end - source_char.start));
    return out;
}

OutputBuffer UnicodeNormalizer::append_compatibility_form(OutputBuffer out, char32_t code_point) {
    // The longest compatibility decomposition is 18 code points (U+FDFA).
    // 32 leaves headroom for future.
    // See https://stackoverflow.com/questions/61779904
    const utf8proc_ssize_t capacity = 32;
    utf8proc_int32_t decomposed[capacity];
    utf8proc_int32_t boundclass = UTF8PROC_BOUNDCLASS_START;
    utf8proc_ssize_t count = utf8proc_decompose_char(
        static_cast<utf8proc_int32_t>(code_point), decomposed, capacity,
        static_cast<utf8proc_option_t>(UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT), &boundclass
    );
    for (utf8proc_ssize_t i = 0; count <= capacity && i < count; ++i) {
        if (diacritics_stripper.is_diacritic_mark(static_cast<char32_t>(decomposed[i]))) {
            return append_without_diacritics(out, code_point);
        }
    }
    if (count > 0 && count <= capacity) {
        count = utf8proc_normalize_utf32(
            decomposed, count, static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE)
        );
    }
    if (count <= 0 || count > capacity) {
        return append_code_point(out, lower(code_point));
    }

    // There can be cases that leave the parentheses:
    // - ⑽ ... ⒇ (U+247D-U+2487) -> "(10)" ... "(20)"
    // - ㈝ (U+321D) -> "(오전)"
    // - ㈞ (U+321E) -> "(오후)"
    if (count == 3 && decomposed[0] == '(' && decomposed[2] == ')') {
        decomposed[0] = decomposed[1];
        count = 1;
    }

    // Folded, not only lowered, so a second pass changes nothing: 𝛓 -> ς -> σ
    for (utf8proc_ssize_t i = 0; i < count; ++i) {
        char32_t part = static_cast<char32_t>(decomposed[i]);
        if (part < 0x80) {
            out.push_back(lower_ascii(static_cast<char>(part)));
        } else if (char_classifier.classify(part) == CharKind::CaseFolding) {
            out = append_case_folded(out, part);
        } else {
            out = append_code_point(out, lower(part));
        }
    }
    return out;
}

OutputBuffer UnicodeNormalizer::append_case_folded(OutputBuffer out, char32_t code_point) {
    // The longest full case folding is 3 code points (ΐ)
    const utf8proc_ssize_t capacity = 4;
    utf8proc_int32_t folded[capacity];
    utf8proc_int32_t boundclass = UTF8PROC_BOUNDCLASS_START;
    utf8proc_ssize_t count = utf8proc_decompose_char(
        static_cast<utf8proc_int32_t>(lower(code_point)), folded, capacity, UTF8PROC_CASEFOLD, &boundclass
    );
    if (count > 1 && count <= capacity) {
        count = utf8proc_normalize_utf32(
            folded, count, static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE)
        );
    }
    if (count <= 0 || count > capacity) {
        return append_code_point(out, lower(code_point));
    }

    for (utf8proc_ssize_t i = 0; i < count; ++i) {
        out = append_code_point(out, static_cast<char32_t>(folded[i]));
    }
    return out;
}

OutputBuffer UnicodeNormalizer::append_without_diacritics(OutputBuffer out, char32_t code_point) {
    char32_t stripped[4];
    int count = diacritics_stripper.strip(code_point, stripped);
    for (int i = 0; i < count; ++i) {
        out = append_code_point(out, stripped[i]);
    }
    return out;
}

OutputBuffer UnicodeNormalizer::append_code_point(OutputBuffer out, char32_t code_point) {
    if (code_point < 0x80) {
        out.push_back(static_cast<char>(code_point));
        return out;
    }
    char latin = ConfusablesMapper::to_latin(code_point);
    if (latin != 0) {
        out.push_back(latin);
        return out;
    }
    utf8proc_uint8_t encoded[4];
    utf8proc_ssize_t encoded_len = utf8proc_encode_char(static_cast<utf8proc_int32_t>(code_point), encoded);
    out.append(reinterpret_cast<const char*>(encoded), static_cast<std::size_t>(encoded_len));
    return out;
}

bool UnicodeNormalizer::is_two_byte_char(std::string_view text, std::ptrdiff_t pos) {
    const utf8proc_uint8_t* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    return bytes[pos] >= 0xC2 && bytes[pos] <= 0xDF && pos + 1 < static_cast<std::ptrdiff_t>(text.size())
        && (bytes[pos + 1] & 0xC0) == 0x80;
}

bool UnicodeNormalizer::is_regional_indicator(char32_t code_point) {
    return code_point >= U'\U0001F1E6' && code_point <= U'\U0001F1FF';
}

bool UnicodeNormalizer::is_regional_indicator_at(std::string_view text, std::ptrdiff_t pos) {
    if (pos < 0 || pos + 4 > static_cast<std::ptrdiff_t>(text.size())) {
        return false;
    }
    utf8proc_int32_t code_point;
    utf8proc_ssize_t len = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data()) + pos, 4, &code_point);
    return len == 4 && is_regional_indicator(static_cast<char32_t>(code_point));
}

char32_t UnicodeNormalizer::last_code_point(std::string_view text) {
    std::size_t start = text.size();
    while (start > 0 && text.size() - start < 4) {
        --start;
        if ((static_cast<unsigned char>(text[start]) & 0xC0) != 0x80) {
            utf8proc_int32_t code_point;
            utf8proc_ssize_t len = utf8proc_iterate(
                reinterpret_cast<const utf8proc_uint8_t*>(text.data()) + start,
                static_cast<utf8proc_ssize_t>(text.size() - start), &code_point
            );
            return len == static_cast<utf8proc_ssize_t>(text.size() - start) ? static_cast<char32_t>(code_point) : 0;
        }
    }
    return 0;
}

char32_t UnicodeNormalizer::lower(char32_t code_point) {
    return static_cast<char32_t>(utf8proc_tolower(static_cast<utf8proc_int32_t>(code_point)));
}

char UnicodeNormalizer::lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

bool UnicodeNormalizer::lower_ascii_block(const char* bytes, char* lowered) {
    const std::uint64_t ones = 0x0101010101010101, high_bits = 0x8080808080808080;
    std::uint64_t chunks[8];
    std::memcpy(chunks, bytes, sizeof(chunks));
    std::uint64_t seen = 0;
    // here we add each byte to the seen bits with OR
    // then, at the IF check, if seen has any non-ASCII char (>= 0x80), it will do an early return
    for (std::uint64_t chunk : chunks) {
        seen |= chunk;
    }
    if ((seen & high_bits) != 0) {
        return false;
    }
    for (std::uint64_t& chunk : chunks) {
        std::uint64_t upper = (chunk + (0x80 - 'A') * ones) & ~(chunk + (0x80 - 'Z' - 1) * ones) & high_bits;
        chunk |= upper >> 2;
    }
    std::memcpy(lowered, chunks, sizeof(chunks));
    return true;
}

const UnicodeNormalizer::FastOutputs& UnicodeNormalizer::fast_outputs() {
    static const FastOutputs outputs = build_fast_outputs();
    return outputs;
}

// 0: needs the full path, 1: copied as is, otherwise the ASCII byte it becomes
UnicodeNormalizer::FastOutputs UnicodeNormalizer::build_fast_outputs() {
    FastOutputs outputs{};
    for (char32_t code_point = 0x80; code_point < outputs.size(); ++code_point) {
        if (
            (code_point >= 0xD800 && code_point < 0xE000)
            || is_regional_indicator(code_point)
            || diacritics_stripper.is_separate_letter_base(code_point)
        ) {
            continue;
        }
        CharKind kind = classify_char({code_point, 0, 0});
        if (kind == CharKind::Regular) {
            char latin = ConfusablesMapper::to_latin(code_point);
            if (latin != 0) {
                outputs[code_point] = static_cast<std::uint8_t>(latin);
            } else if (lower(code_point) == code_point) {
                outputs[code_point] = 1;
            }
            continue;
        }
        std::string output;
        OutputBuffer out(output, 0);
        switch (kind) {
            case CharKind::Space:
                out.push_back(' ');
                break;
            case CharKind::CaseFolding:
                out = append_case_folded(out, code_point);
                break;
            case CharKind::Compatibility:
                out = append_compatibility_form(out, code_point);
                break;
            case CharKind::Diacritic:
                out = append_without_diacritics(out, code_point);
                break;
            default:
                continue;
        }
        out.finish();
        utf8proc_uint8_t source[4];
        utf8proc_ssize_t length = utf8proc_encode_char(static_cast<utf8proc_int32_t>(code_point), source);
        if (output == std::string_view(reinterpret_cast<const char*>(source), static_cast<std::size_t>(length))) {
            outputs[code_point] = 1;
        } else if (output.size() == 1 && static_cast<unsigned char>(output[0]) >= 0x20) {
            outputs[code_point] = static_cast<std::uint8_t>(output[0]);
        }
    }
    return outputs;
}
