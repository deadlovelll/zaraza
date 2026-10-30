#pragma once

#include <array>

#include "confusables_mapper/confusables.hpp"

// Lookalikes of stylized text missing from confusables.txt: ѧss, fuckєd, føck, f•ck. None is a Russian letter.
// Glyphs drawn over a hidden letter become '*', so the decoder reads them as masks; dashes become '-': f–ck
inline constexpr std::array<Confusable, 55> stylized_letters = {{
    {0x00A2, 'c'},    // ¢ CENT SIGN
    {0x00B7, '*'},    // · MIDDLE DOT
    {0x00D7, '*'},    // × MULTIPLICATION SIGN
    {0x00F8, 'o'},    // ø LATIN SMALL LETTER O WITH STROKE
    {0x0111, 'd'},    // đ LATIN SMALL LETTER D WITH STROKE
    {0x0127, 'h'},    // ħ LATIN SMALL LETTER H WITH STROKE
    {0x0142, 'l'},    // ł LATIN SMALL LETTER L WITH STROKE
    {0x0167, 't'},    // ŧ LATIN SMALL LETTER T WITH STROKE
    {0x0180, 'b'},    // ƀ LATIN SMALL LETTER B WITH STROKE
    {0x01AD, 't'},    // ƭ LATIN SMALL LETTER T WITH HOOK
    {0x01B6, 'z'},    // ƶ LATIN SMALL LETTER Z WITH STROKE
    {0x01E5, 'g'},    // ǥ LATIN SMALL LETTER G WITH STROKE
    {0x023C, 'c'},    // ȼ LATIN SMALL LETTER C WITH STROKE
    {0x023F, 's'},    // ȿ LATIN SMALL LETTER S WITH SWASH TAIL
    {0x0247, 'e'},    // ɇ LATIN SMALL LETTER E WITH STROKE
    {0x0249, 'j'},    // ɉ LATIN SMALL LETTER J WITH STROKE
    {0x024D, 'r'},    // ɍ LATIN SMALL LETTER R WITH STROKE
    {0x024F, 'y'},    // ɏ LATIN SMALL LETTER Y WITH STROKE
    {0x0268, 'i'},    // ɨ LATIN SMALL LETTER I WITH STROKE
    {0x0271, 'm'},    // ɱ LATIN SMALL LETTER M WITH HOOK
    {0x0289, 'u'},    // ʉ LATIN SMALL LETTER U BAR
    {0x03B4, 'd'},    // δ GREEK SMALL LETTER DELTA
    {0x03B8, 'o'},    // θ GREEK SMALL LETTER THETA
    {0x03BB, 'a'},    // λ GREEK SMALL LETTER LAMDA
    {0x03BE, 'e'},    // ξ GREEK SMALL LETTER XI
    {0x03C0, 'n'},    // π GREEK SMALL LETTER PI
    {0x03C8, 'y'},    // ψ GREEK SMALL LETTER PSI
    {0x03C9, 'w'},    // ω GREEK SMALL LETTER OMEGA
    {0x0454, 'e'},    // є CYRILLIC SMALL LETTER UKRAINIAN IE
    {0x0467, 'a'},    // ѧ CYRILLIC SMALL LETTER LITTLE YUS
    {0x0493, 'f'},    // ғ CYRILLIC SMALL LETTER GHE WITH STROKE
    {0x04A1, 'k'},    // ҡ CYRILLIC SMALL LETTER BASHKIR KA
    {0x066D, '*'},    // ٭ ARABIC FIVE POINTED STAR
    {0x1D7D, 'p'},    // ᵽ LATIN SMALL LETTER P WITH STROKE
    {0x2013, '-'},    // – EN DASH
    {0x2014, '-'},    // — EM DASH
    {0x2022, '*'},    // • BULLET
    {0x2026, '*'},    // … HORIZONTAL ELLIPSIS
    {0x204E, '*'},    // ⁎ LOW ASTERISK
    {0x2217, '*'},    // ∗ ASTERISK OPERATOR
    {0x22C6, '*'},    // ⋆ STAR OPERATOR
    {0x25A0, '*'},    // ■ BLACK SQUARE
    {0x25CB, '*'},    // ○ WHITE CIRCLE
    {0x25CF, '*'},    // ● BLACK CIRCLE
    {0x2605, '*'},    // ★ BLACK STAR
    {0x2606, '*'},    // ☆ WHITE STAR
    {0x2661, '*'},    // ♡ WHITE HEART SUIT
    {0x2665, '*'},    // ♥ BLACK HEART SUIT
    {0x2716, '*'},    // ✖ HEAVY MULTIPLICATION X
    {0x2731, '*'},    // ✱ HEAVY ASTERISK
    {0x2764, '*'},    // ❤ HEAVY BLACK HEART
    {0x2C65, 'a'},    // ⱥ LATIN SMALL LETTER A WITH STROKE
    {0x2C73, 'w'},    // ⱳ LATIN SMALL LETTER W WITH HOOK
    {0xA741, 'k'},    // ꝁ LATIN SMALL LETTER K WITH STROKE
    {0xA7A5, 'n'},    // ꞥ LATIN SMALL LETTER N WITH OBLIQUE STROKE
}};
