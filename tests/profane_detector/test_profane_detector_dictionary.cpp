#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "profane_detector/profane_detector.hpp"

static ProfaneDetector make_detector(const std::string& words) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / "profane_detector_dictionary_test.txt";
    std::ofstream(path) << words;
    ProfaneDetector detector("", path.string());
    std::filesystem::remove(path);
    return detector;
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
        EXPECT_EQ(detector.censor(padding + "ก่ f u c k сука fuck" + tail, '*'), padding + "** ******* **** ****" + tail) << offset;
    }
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
