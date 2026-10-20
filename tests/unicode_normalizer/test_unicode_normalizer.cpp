#include <gtest/gtest.h>
#include <clocale>
#include <initializer_list>
#include <vector>

#include "combining_marks.hpp"
#include "unicode_normalizer/unicode_normalizer.hpp"

static std::string utf8(char32_t code_point) {
    if (code_point < 0x80) {
        return std::string(1, static_cast<char>(code_point));
    }
    if (code_point < 0x800) {
        return {static_cast<char>(0xC0 | (code_point >> 6)), static_cast<char>(0x80 | (code_point & 0x3F))};
    }
    if (code_point < 0x10000) {
        return {
            static_cast<char>(0xE0 | (code_point >> 12)), static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)),
            static_cast<char>(0x80 | (code_point & 0x3F))
        };
    }
    return {
        static_cast<char>(0xF0 | (code_point >> 18)), static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)),
        static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)), static_cast<char>(0x80 | (code_point & 0x3F))
    };
}

class UnicodeNormalizerTest : public ::testing::Test {
protected:
    UnicodeNormalizer normalizer;
};

TEST_F(UnicodeNormalizerTest, AllUppercase) {
    std::string text = "HELLO";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "hello");
}

TEST_F(UnicodeNormalizerTest, MixedCase) {
    std::string text = "HeLLo WoRLd";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "hello world");
}

TEST_F(UnicodeNormalizerTest, AlreadyLowercase) {
    std::string text = "already lowercase";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "already lowercase");
}

TEST_F(UnicodeNormalizerTest, NumbersAndSymbolsUnchanged) {
    std::string text = "1234!@#$%^&*()";
    std::string expected = text;
    text = normalizer.normalize(text);
    EXPECT_EQ(text, expected);
}

TEST_F(UnicodeNormalizerTest, MixedLettersAndSymbols) {
    std::string text = "Sh!Th0u$e @2026";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "sh!th0u$e @2026");
}

TEST_F(UnicodeNormalizerTest, EmptyString) {
    std::string text = "";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "");
}

TEST_F(UnicodeNormalizerTest, CyrillicUppercase) {
    std::string text = "БЛЯТЬ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "бляtb");  // Т and Ь look like Latin T and b
}

TEST_F(UnicodeNormalizerTest, CyrillicAndLatinMixed) {
    std::string text = "ПрИвЕт WoRLd";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "пpиbet world");  // пpиbet world: р, в, Е, т look like Latin p, b, e, t
}

TEST_F(UnicodeNormalizerTest, LatinDiacriticsStripped) {
    std::string text = "ÜBER ÇA ÉTÉ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "uber ca ete");
}

TEST_F(UnicodeNormalizerTest, Greek) {
    std::string text = "ΣΟΦΊΑ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "ooφia");  // ooφia: σ, Ο, ι, Α look like Latin o, o, i, a
}

TEST_F(UnicodeNormalizerTest, TurkishDottedCapitalI) {
    std::string text = "İ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "i");
}

TEST_F(UnicodeNormalizerTest, ByteLengthChanges) {
    // U+023A (2 bytes) lowercases to U+2C65 (3 bytes)
    std::string text = "Ⱥ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "ⱥ");
}

TEST_F(UnicodeNormalizerTest, FullwidthLetters) {
    std::string text = "ＦＵＣＫ ｆｕｃｋ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck");
}

TEST_F(UnicodeNormalizerTest, EmojiAndCjkUnchanged) {
    std::string text = "😀 漢字";
    std::string expected = text;
    text = normalizer.normalize(text);
    EXPECT_EQ(text, expected);
}

TEST_F(UnicodeNormalizerTest, InvalidUtf8BytesPreserved) {
    // stray continuation byte, invalid 0xFF and a truncated 2-byte sequence at the end
    std::string text = std::string("A\x80" "B\xFF" "C\xC3", 6);
    text = normalizer.normalize(text);
    EXPECT_EQ(text, std::string("a\x80" "b\xFF" "c\xC3", 6));
}

TEST_F(UnicodeNormalizerTest, AllAsciiCharacters) {
    std::string text;
    std::string expected;
    for (int c = 0; c < 128; ++c) {
        text.push_back(static_cast<char>(c));
        expected.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
    }
    text = normalizer.normalize(text);
    EXPECT_EQ(text, expected);
}

TEST_F(UnicodeNormalizerTest, IndependentOfTurkishLocale) {
    const char* previous = std::setlocale(LC_CTYPE, nullptr);
    std::string saved = previous ? previous : "C";

    const char* turkish = nullptr;
    for (const char* name : {"tr_TR.ISO8859-9", "tr_TR.ISO-8859-9", "tr_TR", "tr_TR.UTF-8"}) {
        turkish = std::setlocale(LC_CTYPE, name);
        if (turkish) break;
    }
    if (!turkish) {
        GTEST_SKIP() << "no Turkish locale installed";
    }

    std::string text = "I";
    text = normalizer.normalize(text);
    std::setlocale(LC_CTYPE, saved.c_str());
    EXPECT_EQ(text, "i");
}

TEST_F(UnicodeNormalizerTest, PlainAsciiUnchanged) {
    std::string text = "fuck you";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck you");
}

TEST_F(UnicodeNormalizerTest, OnlyInvisibleChars) {
    std::string text = "​‌‍﻿";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "");
}

TEST_F(UnicodeNormalizerTest, ZeroWidthSpace) {
    std::string text = "f​u​c​k";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, ZeroWidthJoinerAndNonJoiner) {
    std::string text = "f‌uc‍k";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, SoftHyphen) {
    std::string text = "fu­ck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, WordJoiner) {
    std::string text = "sh⁠it";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "shit");
}

TEST_F(UnicodeNormalizerTest, ByteOrderMarkAtStartAndInside) {
    std::string text = "﻿fu﻿ck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, BidiEmbeddingsAndOverrides) {
    std::string text = "‪fu‮ck‬";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, BidiIsolatesAndMarks) {
    std::string text = "⁦f‎u‏c⁩k";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, VariationSelectors) {
    std::string text = "f️u︀ck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, VariationSelectorSupplement) {
    std::string text = "fu\U000E0100ck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, TagCharacters) {
    std::string text = "f\U000E0001u\U000E0041ck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, HangulFiller) {
    std::string text = "fㅤuck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, CyrillicAndEmojiKept) {
    std::string text = "дядя 😀 漢字";
    std::string expected = text;
    text = normalizer.normalize(text);
    EXPECT_EQ(text, expected);
}

TEST_F(UnicodeNormalizerTest, EmojiVariationSelectorRemoved) {
    std::string text = "❤️";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "❤");
}

TEST_F(UnicodeNormalizerTest, WhitespaceKeptNoBreakSpaceBecomesSpace) {
    std::string text = "a b\tc\nd\u00A0e";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "a b\tc\nd e");
}

TEST_F(UnicodeNormalizerTest, InvisibleCharsInsideUppercaseLatin) {
    std::string text = "F\u200BU\u00ADC\u2060K";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, InvisibleCharsInsideUppercaseCyrillic) {
    std::string text = "\uFEFFСУ\u200BКА\u202E";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "cyka");  // С, У, К, А look like Latin C, y, K, A
}

TEST_F(UnicodeNormalizerTest, MathematicalBoldItalicScript) {
    std::string text = "𝐟𝐮𝐜𝐤 𝙛𝙪𝙘𝙠 𝓯𝓾𝓬𝓴";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck fuck");
}

TEST_F(UnicodeNormalizerTest, MathematicalFrakturDoubleStruckMonospace) {
    std::string text = "𝔣𝔲𝔠𝔨 𝕗𝕦𝕔𝕜 𝚏𝚞𝚌𝚔";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck fuck");
}

TEST_F(UnicodeNormalizerTest, MathematicalBoldCapitalsAreLowered) {
    std::string text = "𝐅𝐔𝐂𝐊";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, CircledLetters) {
    std::string text = "ⓕⓤⓒⓚ ⒻⓊⒸⓀ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck");
}

TEST_F(UnicodeNormalizerTest, SquaredLetters) {
    std::string text = "🄵🅄🄲🄺";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, ParenthesizedLettersLoseParentheses) {
    std::string text = "⒡⒰⒞⒦ 🄕🄤🄒🄚";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck");
}

TEST_F(UnicodeNormalizerTest, ParenthesizedMultiDigitNumberKept) {
    std::string text = "⑽";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "(10)");
}

TEST_F(UnicodeNormalizerTest, SuperscriptModifierLetters) {
    std::string text = "ᶠᵘᶜᵏ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, Ligatures) {
    std::string text = "ﬁnger ﬀ ﬆ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "finger ff st");
}

TEST_F(UnicodeNormalizerTest, MiscCompatibilitySymbols) {
    std::string text = "™ ½ ㎏ Ⅳ ① ℌ ſ …";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "tm 1\u20442 kg iv 1 h s ...");
}

TEST_F(UnicodeNormalizerTest, SpecialSpacesBecomeSpace) {
    std::string text = "a\u2002b\u3000c";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "a b c");
}

TEST_F(UnicodeNormalizerTest, SeparatorsAndBrailleBlankBecomeSpace) {
    std::string text = "a\u2000b\u1680c\u2028d\u2029e\u2800f";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "a b c d e f");
}

TEST_F(UnicodeNormalizerTest, CompatibilityLettersWithInvisibleChars) {
    std::string text = "Ｓ\u200BＨ\u00ADＩＴ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "shit");
}

TEST_F(UnicodeNormalizerTest, TitlecaseDigraphLosesCaron) {
    std::string text = "ǅ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "dz");
}

TEST_F(UnicodeNormalizerTest, SharpS) {
    std::string text = "ß ẞ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "ss ss");
}

TEST_F(UnicodeNormalizerTest, SharpSInsideWords) {
    std::string text = "STRAẞE Straße";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "strasse strasse");
}

TEST_F(UnicodeNormalizerTest, GreekFinalSigma) {
    std::string text = "ΚΟΣΜΟΣ κόσμος";
    text = normalizer.normalize(text);
    // koomoo koomoo: Σ and ς fold to σ, which looks like o. Κ and Μ look like K and M, small κ and μ follow them
    EXPECT_EQ(text, "koomoo koomoo");
}

TEST_F(UnicodeNormalizerTest, GreekCapitalWithProsgegrammeni) {
    std::string text = "ᾈ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "a");
}

TEST_F(UnicodeNormalizerTest, CherokeeFoldsToCapitals) {
    std::string text = "ꭰ Ꭰ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "d d");  // both fold to Ꭰ, which looks like Latin D
}

TEST_F(UnicodeNormalizerTest, KelvinAndAngstromSigns) {
    std::string text = "\u212A \u212B";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "k a");
}

TEST_F(UnicodeNormalizerTest, AccentedObfuscation) {
    std::string text = "fùck fúck fûck fück fŭck fǔck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck fuck fuck fuck fuck");
}

TEST_F(UnicodeNormalizerTest, MultipleMarksOnOneLetter) {
    std::string text = "ǖ ǛȐ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "u ur");
}

TEST_F(UnicodeNormalizerTest, CyrillicYoBecomesYe) {
    std::string text = "ЁБАНЫЙ ёб";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "eбahый eб");  // eбahый eб: е, А, Н look like Latin e, A, H
}

TEST_F(UnicodeNormalizerTest, CyrillicLettersWithMarkKept) {
    std::string text = "хуй ХУЙ ў Ў ї Ї";
    text = normalizer.normalize(text);
    // xyй xyй ў ў ї ї: х and у look like Latin x and y, й, ў, ї keep their marks
    EXPECT_EQ(text, "xyй xyй ў ў ї ї");
}

TEST_F(UnicodeNormalizerTest, DecomposedCyrillicLettersWithMarkKept) {
    std::string text = "хуи\u0306 ХУИ\u0306 у\u0306 У\u0306 і\u0308 І\u0308";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "xyй xyй ў ў ї ї");
}

TEST_F(UnicodeNormalizerTest, SourceOffsetsPointToOriginalCharacters) {
    std::string text = "Ｆu​ß";
    EXPECT_EQ(normalizer.normalize(text), "fuss");
    EXPECT_EQ(normalizer.map_to_source(text, {0, 1, 2, 3, 4}), (std::vector<std::size_t>{0, 3, 7, 7, 9}));
    EXPECT_EQ(normalizer.map_to_source(text, {1, 3}), (std::vector<std::size_t>{3, 7}));

    std::string decomposed = "хй";
    EXPECT_EQ(normalizer.map_to_source(decomposed, {0, 1, 2, 3}), (std::vector<std::size_t>{0, 2, 2, 6}));
}

TEST_F(UnicodeNormalizerTest, StrikethroughAndUnderline) {
    std::string text = "f\u0336u\u0336c\u0336k\u0336 s\u0332h\u0332i\u0332t\u0332";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck shit");
}

TEST_F(UnicodeNormalizerTest, ZalgoText) {
    std::string text = "f\u0315\u0321\u0358u\u035Ec\u0489\u0483k";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, DecomposedAccentsStripped) {
    std::string text = "fu\u0300ck E\u0301TE\u0301";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck ete");
}

TEST_F(UnicodeNormalizerTest, IndicMarksKept) {
    std::string text = "चूतिया क़";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "चूतिया क़");
}

TEST_F(UnicodeNormalizerTest, MarksOfOtherScriptsRemoved) {
    std::string text = "s\u0E48hit s\u0E48\u0E48hit f\u064Fuck с\u0650ука क\u0E48 s\u093Ehit";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "shit shit fuck cyka क shit");
}

TEST_F(UnicodeNormalizerTest, MarksOfOwnScriptKept) {
    std::string text = "ไม้ ส้้ كُسّ か\u3099 क\u093E";
    std::string expected = text;
    text = normalizer.normalize(text);
    EXPECT_EQ(text, expected);
}

TEST_F(UnicodeNormalizerTest, EveryMarkRemovedInsideLatinWord) {
    std::vector<char32_t> kept;
    for (const CombiningMarks& marks : combining_marks) {
        for (char32_t mark = marks.first; mark <= marks.last; ++mark) {
            std::string text = "s" + utf8(mark) + "hit";
            text = normalizer.normalize(text);
            if (text != "shit") {
                kept.push_back(mark);
            }
        }
    }
    EXPECT_EQ(kept, std::vector<char32_t>{});
}

TEST_F(UnicodeNormalizerTest, EveryMarkKeptAfterLetterOfItsScript) {
    std::vector<char32_t> removed;
    for (const CombiningMarks& marks : combining_marks) {
        for (char32_t mark = marks.first; marks.letter != 0 && mark <= marks.last; ++mark) {
            std::string expected = utf8(marks.letter);
            expected = normalizer.normalize(expected);
            std::string text = utf8(marks.letter) + utf8(mark);
            text = normalizer.normalize(text);
            if (text != expected + utf8(mark)) {
                removed.push_back(mark);
            }
        }
    }
    EXPECT_EQ(removed, std::vector<char32_t>{});
}

TEST_F(UnicodeNormalizerTest, HangulUnchanged) {
    std::string text = "한국어";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "한국어");
}

TEST_F(UnicodeNormalizerTest, ChurchSlavonicCombiningMarks) {
    std::string text = "сⷠу꙰к꙼аꚞ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "cyka");  // cyka: с, у, к, а look like Latin c, y, k, a
}

TEST_F(UnicodeNormalizerTest, CyrillicLookalikeInLatinWord) {
    std::string text = "fuсk";  // Cyrillic с
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, UppercaseCyrillicLookalikes) {
    std::string text = "ВІТСН";  // ВІТСН, all Cyrillic
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "bitch");
}

TEST_F(UnicodeNormalizerTest, GreekLookalikes) {
    std::string text = "ΤΙΤ";  // ΤΙΤ, all Greek
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "tit");
}

TEST_F(UnicodeNormalizerTest, LookalikeWithDiacritic) {
    std::string text = "fùсk";  // ù and Cyrillic с
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, LookalikeAfterCaseFolding) {
    std::string text = "ꭰ";  // Cherokee small letter a folds to Ꭰ, which looks like D
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "d");
}

TEST_F(UnicodeNormalizerTest, CyrillicWordWithoutLookalikesUnchanged) {
    std::string text = "дядя";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "дядя");
}

TEST_F(UnicodeNormalizerTest, LookalikesIndependentOfCase) {
    std::string text = "сука СУКА Сука";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "cyka cyka cyka");
}

TEST_F(UnicodeNormalizerTest, SmallCapitals) {
    std::string text = "ꜰᴜᴄᴋ";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, NegativeCircledLetters) {
    std::string text = "🅕🅤🅒🅚";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, NegativeSquaredLetters) {
    std::string text = "🅵🆄🅲🅺";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, RegionalIndicatorsSeparatedByInvisibleChars) {
    std::string text = "\U0001F1EB​\U0001F1FA​\U0001F1E8​\U0001F1F0";  // 🇫 🇺 🇨 🇰 split by ZWSP
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, RegionalIndicatorsSeparatedBySpaces) {
    std::string text = "\U0001F1EB \U0001F1FA \U0001F1E8 \U0001F1F0";  // 🇫 🇺 🇨 🇰
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "f u c k");
}

TEST_F(UnicodeNormalizerTest, RegionalIndicatorNextToLetters) {
    std::string text = "\U0001F1EBuck";  // 🇫uck
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck");
}

TEST_F(UnicodeNormalizerTest, FlagsKept) {
    std::string text = "\U0001F1FA\U0001F1F8 \U0001F1E6\U0001F1F7\U0001F1F8\U0001F1EA";  // 🇺🇸 🇦🇷🇸🇪, letters would spell arse
    std::string expected = text;
    text = normalizer.normalize(text);
    EXPECT_EQ(text, expected);
}

TEST_F(UnicodeNormalizerTest, SpacingAccentsInsideWord) {
    std::string text = "f\u00B4uck fu\u02DCck";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "fuck fuck");
}

TEST_F(UnicodeNormalizerTest, CaseFoldingLosesDiacritics) {
    std::string text = "\u01F0erk \u1E97wat \u0390";
    text = normalizer.normalize(text);
    EXPECT_EQ(text, "jerk twat i");
}
