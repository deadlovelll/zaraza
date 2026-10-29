#include <gtest/gtest.h>
#include "profane_detector/profane_detector.hpp"

class ProfaneDetectorTest : public ::testing::Test {
protected:
    ProfaneDetectorTest() 
        : detector("en", "") {}

    ProfaneDetector detector;
};

TEST_F(ProfaneDetectorTest, DoesNotCensorHelloWord) {
    std::string hello_word = "hello";
    std::string censored = detector.censor(hello_word);
    EXPECT_EQ(censored, hello_word) << "Word was incorrectly censored: " << hello_word;
}

TEST_F(ProfaneDetectorTest, DoesNotCensorCatWord) {
    std::string cat_word = "cat";
    std::string censored = detector.censor(cat_word);
    EXPECT_EQ(censored, cat_word) << "Word was incorrectly censored: " << cat_word;
}

TEST_F(ProfaneDetectorTest, DoesNotCensorDadWord) {
    std::string dad_word = "dad";
    std::string censored = detector.censor(dad_word);
    EXPECT_EQ(censored, dad_word) << "Word was incorrectly censored: " << dad_word;
}

TEST_F(ProfaneDetectorTest, DoesNotCensorPotWord) {
    std::string pot_word = "pot";
    std::string pots_word = "pots";
    std::string censored_pot = detector.censor(pot_word);
    std::string censored_pots = detector.censor(pot_word);
    EXPECT_EQ(censored_pot, pot_word) << "Word was incorrectly censored: " << pot_word;
    EXPECT_EQ(censored_pots, pot_word) << "Word was incorrectly censored: " << pot_word;
}

TEST_F(ProfaneDetectorTest, DoesNotCensorTeenWord) {
    std::string teen_word = "teen";
    std::string teens_word = "teens";
    std::string censored_teen = detector.censor(teen_word);
    std::string censored_teens = detector.censor(teens_word);
    EXPECT_EQ(censored_teen, teen_word) << "Word was incorrectly censored: " << teen_word;
    EXPECT_EQ(censored_teens, teens_word) << "Word was incorrectly censored: " << teens_word;
}

TEST_F(ProfaneDetectorTest, DoesNotCensorMenWord) {
    std::string men_word = "men";
    std::string mens_word = "mens";
    std::string censored_men = detector.censor(men_word);
    std::string censored_mens = detector.censor(mens_word);
    EXPECT_EQ(censored_men, men_word) << "Word was incorrectly censored: " << men_word;
    EXPECT_EQ(censored_mens, mens_word) << "Word was incorrectly censored: " << mens_word;
}

TEST_F(ProfaneDetectorTest, DoesNotCensorLegWord) {
    std::string leg_word = "leg";
    std::string legs_word = "legs";
    std::string censored_leg = detector.censor(leg_word);
    std::string censored_legs = detector.censor(legs_word);
    EXPECT_EQ(censored_leg, leg_word) << "Word was incorrectly censored: " << leg_word;
    EXPECT_EQ(censored_legs, legs_word) << "Word was incorrectly censored: " << legs_word;
}