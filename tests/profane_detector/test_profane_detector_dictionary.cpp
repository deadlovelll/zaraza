#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "profane_detector/profane_detector.hpp"

static ProfaneDetector make_detector(const std::string& words) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / "profane_detector_dictionary_test.txt";
    std::ofstream(path) << words;
    ProfaneDetector detector("", path.string());
    std::filesystem::remove(path);
    return detector;
}

static std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

TEST(ProfaneDetectorDictionaryTest, NonAsciiDictionaryWordFound) {
    ProfaneDetector detector = make_detector("сука\n");
    for (const std::string& text : {"Сука", "СУКА", "сука"}) {
        std::string censored = detector.censor(text, '*');
        EXPECT_EQ(censored.find_first_not_of('*'), std::string::npos) << text << " -> " << censored;
    }
}

TEST(ProfaneDetectorDictionaryTest, SpacedLettersFound) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_EQ(detector.censor("you f u c k off", '*'), "you ******* off");
    EXPECT_EQ(detector.censor("f.u.c.k", '*'), "*******");
}

TEST(ProfaneDetectorDictionaryTest, PhraseAcrossBatchBoundaryFound) {
    ProfaneDetector detector = make_detector("son of a bitch\n");
    std::string padding(65530, 'x');
    EXPECT_EQ(detector.censor(padding + " son of a bitch", '*'), padding + " *** ** * *****");
    EXPECT_TRUE(detector.contains_profanity(padding + " son of a bitch"));
}

TEST(ProfaneDetectorDictionaryTest, WordsBetweenHyphensAndUnderscoresFound) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_EQ(detector.censor("-fuck- _fuck_", '*'), "-****- _****_");
}

TEST(ProfaneDetectorDictionaryTest, LatinPartOfNonLatinWordNotFound) {
    ProfaneDetector detector = make_detector("wop\n");
    EXPECT_EQ(detector.censor("шорты", '*').find('*'), std::string::npos);
}

TEST(ProfaneDetectorDictionaryTest, PhraseAcrossPunctuationAndNewlinesFound) {
    ProfaneDetector detector = make_detector("alabama hot pocket\n");
    EXPECT_EQ(detector.censor("alabama,  hot\npocket", '*'), "*******,  ***\n******");
}

TEST(ProfaneDetectorDictionaryTest, HyphenatedDictionaryEntryFound) {
    ProfaneDetector detector = make_detector("arse-licking\n");
    EXPECT_EQ(detector.censor("arse licking", '*'), "**** *******");
    EXPECT_EQ(detector.censor("arse-licking", '*'), "****-*******");
}

TEST(ProfaneDetectorDictionaryTest, CensorKeepsOriginalText) {
    ProfaneDetector detector = make_detector("fuck\nсука\n");
    EXPECT_EQ(detector.censor("Привет, как дела? FUCK you, СУКА!", '*'), "Привет, как дела? **** you, ****!");
}

TEST(ProfaneDetectorDictionaryTest, InvisibleCharsAndMarksInsideWordMasked) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_EQ(detector.censor("fu​ck f̶u̶c̶k̶!", '*'), "**** ****!");
    EXPECT_EQ(detector.censor("fu\u0E48ck", '*'), "****");
}

TEST(ProfaneDetectorDictionaryTest, OneCensorCharPerOriginalCharacter) {
    ProfaneDetector detector = make_detector("ass\noff\n");
    EXPECT_EQ(detector.censor("aß", '*'), "**");
    EXPECT_EQ(detector.censor("o\uFB00", '*'), "**");
}

TEST(ProfaneDetectorDictionaryTest, ThreeAndFourByteLettersFound) {
    ProfaneDetector detector = make_detector("漢字\n𠜎𠜱\n");
    EXPECT_EQ(detector.censor("漢字 𠜎𠜱 漢𠜎", '*'), "** ** 漢𠜎");
}

TEST(ProfaneDetectorDictionaryTest, DigitsInDictionaryWordFound) {
    ProfaneDetector detector = make_detector("a55hole\n");
    EXPECT_EQ(detector.censor("you A55HOLE", '*'), "you *******");
    EXPECT_EQ(detector.censor("asshole a55", '*'), "asshole a55");
}

TEST(ProfaneDetectorDictionaryTest, NonAsciiWordAfterFailLinkFound) {
    ProfaneDetector detector = make_detector("ab cd ef\ncd жук\n");
    EXPECT_EQ(detector.censor("ab cd жук", '*'), "ab ** ***");
}

TEST(ProfaneDetectorDictionaryTest, WordsAcrossStreamingBatchesFound) {
    ProfaneDetector detector = make_detector("fuck\nсука\nson of a bitch\nก่\n");
    std::string tail;
    while (tail.size() < 200000) {
        tail += " ab";
    }
    for (std::size_t offset : {65528u, 65530u, 65533u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u}) {
        std::string padding(offset % 3, 'x');
        while (padding.size() < offset) {
            padding += "ab ";
        }
        EXPECT_EQ(detector.censor(padding + "s o n of a bitch" + tail, '*'), padding + "***** ** * *****" + tail) << offset;
        EXPECT_EQ(detector.censor(padding + "son of a bitch" + tail, '*'), padding + "*** ** * *****" + tail) << offset;
        EXPECT_EQ(detector.censor(padding + "ก่ f u c k сука fuck" + tail, '*'), padding + "** ******* **** ****" + tail) << offset;
    }
}

TEST(ProfaneDetectorDictionaryTest, SkippedWordBreaksPhrase) {
    ProfaneDetector detector = make_detector("hot\npocket\nhot pocket\n");
    EXPECT_EQ(detector.censor("hot dog pocket", '*'), "*** dog ******");
    EXPECT_EQ(detector.censor("hot pocket", '*'), "*** ******");
    std::string tail;
    while (tail.size() < 200000) {
        tail += " ab";
    }
    for (std::size_t offset = 65520; offset < 65540; ++offset) {
        std::string padding(offset % 3, 'x');
        while (padding.size() < offset) {
            padding += "ab ";
        }
        EXPECT_EQ(detector.censor(padding + "hot dog pocket" + tail, '*'), padding + "*** dog ******" + tail) << offset;
    }
}

TEST(ProfaneDetectorDictionaryTest, PhraseAcrossTokenBlocksFound) {
    ProfaneDetector detector = make_detector("son of a bitch\nfuck off\n");
    for (std::size_t count = 56; count < 72; ++count) {
        std::string padding;
        for (std::size_t i = 0; i < count; ++i) {
            padding += "ab ";
        }
        EXPECT_EQ(detector.censor(padding + "son of a bitch ab", '*'), padding + "*** ** * ***** ab") << count;
        EXPECT_EQ(detector.censor(padding + "f.u.c.k off ab", '*'), padding + "******* *** ab") << count;
    }
}

TEST(ProfaneDetectorDictionaryTest, LongDictionaryWordsFound) {
    ProfaneDetector detector = make_detector("motherfucker\nmotherfuckingasshole\nfat motherfucking bastard\n");
    EXPECT_EQ(detector.censor("motherfucker", '*'), "************");
    EXPECT_EQ(detector.censor("you motherfucker!", '*'), "you ************!");
    EXPECT_EQ(detector.censor("a motherfuckingasshole", '*'), "a ********************");
    EXPECT_EQ(detector.censor("motherfuckingasshole ok", '*'), "******************** ok");
    EXPECT_EQ(detector.censor("so fat motherfucking bastard ok", '*'), "so *** ************* ******* ok");
    EXPECT_EQ(detector.censor("motherfuckingarsehole motherfuckers", '*'), "motherfuckingarsehole motherfuckers");
}

TEST(ProfaneDetectorDictionaryTest, WordsAfterManyStreamingBatchesFound) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::string text;
    std::string censored;
    while (text.size() < 1000000) {
        text += "café fuck ";
        censored += "café **** ";
    }
    EXPECT_EQ(detector.censor(text, '*'), censored);
}

TEST(ProfaneDetectorDictionaryTest, TokensLongerThanBatchKeptWhole) {
    ProfaneDetector detector = make_detector("fuck\n");
    for (std::size_t length : {65536u, 131072u, 262144u, 300000u}) {
        std::string token(length, 'x');
        EXPECT_EQ(detector.censor(token + "fuck fuck", '*'), token + "fuck ****") << length;
        EXPECT_FALSE(detector.contains_profanity(token + "fuck")) << length;
    }
}

TEST(ProfaneDetectorDictionaryTest, ContainsProfanity) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_TRUE(detector.contains_profanity("F.U.C.K"));
    EXPECT_FALSE(detector.contains_profanity("hello"));
}

TEST(ProfaneDetectorDictionaryTest, CensorReportsWhetherTextChanged) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::string censored = "untouched";
    EXPECT_FALSE(detector.censor("hello there", '*', censored));
    EXPECT_EQ(censored, "untouched");
    EXPECT_TRUE(detector.censor("hello fuck", '*', censored));
    EXPECT_EQ(censored, "hello ****");
}

TEST(ProfaneDetectorDictionaryTest, CensorInplaceMatchesCensor) {
    ProfaneDetector detector = make_detector("fuck\nсука\nson of a bitch\nass\n");
    std::string tail;
    while (tail.size() < 200000) {
        tail += " ab сука";
    }
    std::vector<std::string> texts = {
        "",
        "hello",
        "fuck",
        "you fuck off",
        "Сука, fuck и СУКА",
        "сука ok",
        "aß s o n of a bitch f.u.c.k",
        "сука" + tail,
        tail + " fuck",
    };
    for (const std::string& text : texts) {
        std::string buffer = text;
        std::size_t size = detector.censor_inplace(buffer, '*');
        EXPECT_EQ(buffer.substr(0, size), detector.censor(text, '*')) << text.substr(0, 40);
    }
}

TEST(ProfaneDetectorDictionaryTest, TokenEndingInsideExpansionGetsNoExtraStar) {
    ProfaneDetector detector = make_detector("xa1\n");
    std::string buffer = "xa\u2488";
    EXPECT_EQ(detector.censor(buffer, '*'), "**\u2488");
    std::size_t size = detector.censor_inplace(buffer, '*');
    EXPECT_EQ(buffer.substr(0, size), "**\u2488");
}

TEST(ProfaneDetectorDictionaryTest, CensorFileMatchesCensor) {
    ProfaneDetector detector = make_detector("fuck\nсука\nson of a bitch\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_test.txt";
    std::string tail;
    while (tail.size() < 300000) {
        tail += " ab сука fuck";
    }
    std::vector<std::string> texts = {"", "hello", "you fuck off", "сука ok", "\xEF\xBB\xBF" "fuck", "son of a bitch" + tail};
    for (const std::string& text : texts) {
        std::ofstream(path, std::ios::binary) << text;
        std::size_t size = detector.censor_file(path, '*');
        EXPECT_EQ(read_file(path), detector.censor(text, '*')) << text.substr(0, 20);
        EXPECT_EQ(size, std::filesystem::file_size(path));
    }
    std::filesystem::remove(path);
}

static std::string utf16_bytes(const std::u16string& text, bool big_endian) {
    std::string bytes = big_endian ? "\xFE\xFF" : "\xFF\xFE";
    for (char16_t unit : text) {
        char high = static_cast<char>(unit >> 8);
        char low = static_cast<char>(unit & 0xFF);
        bytes += big_endian ? std::string{high, low} : std::string{low, high};
    }
    return bytes;
}

TEST(ProfaneDetectorDictionaryTest, CensorFileKeepsUtf16) {
    ProfaneDetector detector = make_detector("fuck\nсука\nсука1\n𠜎𠜱\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_utf16_test.txt";
    std::u16string tail;
    std::u16string censored_tail;
    while (tail.size() < 200000) {
        tail += u" ab сука fuck";
        censored_tail += u" ab **** ****";
    }
    std::vector<std::pair<std::u16string, std::u16string>> cases = {
        {u"", u""},
        {u"You FUCK off, сука! 😀", u"You **** off, ****! 😀"},
        {u"сука\u2488", u"****\u2488"},
        {u"𠜎𠜱 ok", u"** ok"},
        {u"start" + tail, u"start" + censored_tail},
    };
    for (bool big_endian : {false, true}) {
        for (const std::pair<std::u16string, std::u16string>& item : cases) {
            std::ofstream(path, std::ios::binary) << utf16_bytes(item.first, big_endian);
            std::size_t size = detector.censor_file(path, '*');
            EXPECT_EQ(read_file(path), utf16_bytes(item.second, big_endian)) << big_endian << " " << item.first.size();
            EXPECT_EQ(size, std::filesystem::file_size(path));
        }
    }
    std::ofstream(path, std::ios::binary) << utf16_bytes(u"сука ok", false) + "x";
    detector.censor_file(path, '*');
    EXPECT_EQ(read_file(path), utf16_bytes(u"**** ok", false) + "x");
    std::string lone = utf16_bytes(std::u16string(u"fuck ") + char16_t(0xD800) + u"a", false);
    std::ofstream(path, std::ios::binary) << lone;
    EXPECT_THROW(detector.censor_file(path, '*'), std::invalid_argument);
    EXPECT_EQ(read_file(path), lone);
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileRejectsUtf32AndMissingFiles) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_utf32_test.txt";
    for (std::string utf32 : {std::string("\xFF\xFE\0\0" "f\0\0\0", 8), std::string("\0\0\xFE\xFF" "\0\0\0f", 8)}) {
        std::ofstream(path, std::ios::binary) << utf32;
        EXPECT_THROW(detector.censor_file(path, '*'), std::invalid_argument);
        EXPECT_EQ(read_file(path), utf32);
    }
    std::filesystem::remove(path);
    EXPECT_THROW(detector.censor_file(path, '*'), std::system_error);
}
