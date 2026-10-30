#include <gtest/gtest.h>

#include "char_classifier/char_classifier.hpp"

class CharClassifierTest : public ::testing::Test {
protected:
    CharClassifier classifier;
};

TEST_F(CharClassifierTest, RegularLetters) {
    EXPECT_EQ(classifier.classify(U'a'), CharKind::Regular);
    EXPECT_EQ(classifier.classify(U'Ж'), CharKind::Regular);
    EXPECT_EQ(classifier.classify(U'漢'), CharKind::Regular);
}

TEST_F(CharClassifierTest, InvisibleCharacters) {
    EXPECT_EQ(classifier.classify(U'​'), CharKind::Invisible);  // zero width space
    EXPECT_EQ(classifier.classify(U'­'), CharKind::Invisible);  // soft hyphen
    EXPECT_EQ(classifier.classify(0x0600), CharKind::Invisible);  // Arabic number sign, format
    EXPECT_EQ(classifier.classify(0x0092), CharKind::Invisible);  // C1 control
    EXPECT_EQ(classifier.classify(0x2065), CharKind::Invisible);  // unassigned ignorable
    EXPECT_EQ(classifier.classify(0xE0FFF), CharKind::Invisible);
    EXPECT_EQ(classifier.classify(0x02BB), CharKind::Invisible);  // modifier letter turned comma
    EXPECT_EQ(classifier.classify(0x0085), CharKind::Space);  // next line
}

TEST_F(CharClassifierTest, SpaceCharacters) {
    EXPECT_EQ(classifier.classify(U' '), CharKind::Space);
    EXPECT_EQ(classifier.classify(U' '), CharKind::Space);
    EXPECT_EQ(classifier.classify(U' '), CharKind::Space);
    EXPECT_EQ(classifier.classify(U' '), CharKind::Space);
    EXPECT_EQ(classifier.classify(U'⠀'), CharKind::Space);
}

TEST_F(CharClassifierTest, CompatibilityCharacters) {
    EXPECT_EQ(classifier.classify(U'Ｆ'), CharKind::Compatibility);
    EXPECT_EQ(classifier.classify(U'ﬁ'), CharKind::Compatibility);
}

TEST_F(CharClassifierTest, FoldDiffersFromLowercase) {
    EXPECT_EQ(classifier.classify(U'ß'), CharKind::CaseFolding);
}

TEST_F(CharClassifierTest, AccentedLetters) {
    EXPECT_EQ(classifier.classify(U'é'), CharKind::Diacritic);
    EXPECT_EQ(classifier.classify(U'Ё'), CharKind::Diacritic);
}

TEST_F(CharClassifierTest, StandaloneMarks) {
    EXPECT_EQ(classifier.classify(U'́'), CharKind::Diacritic);  // combining acute accent
    EXPECT_EQ(classifier.classify(U'̶'), CharKind::Diacritic);  // combining long stroke overlay
    EXPECT_EQ(classifier.classify(U'⃝'), CharKind::Diacritic);  // combining enclosing circle
}

TEST_F(CharClassifierTest, IndicMarkIsMark) {
    EXPECT_EQ(classifier.classify(U'़'), CharKind::Mark);  // Devanagari nukta
    EXPECT_EQ(classifier.classify(U'\u093E'), CharKind::Mark);
}
