#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "tokenizer/tokenizer.hpp"

class TokenizerTest : public ::testing::Test {
protected:
    Tokenizer tokenizer;

    std::string text_of(const std::string& text, const Token& token) {
        std::string letters = text.substr(token.start, token.end - token.start);
        std::erase(letters, token.separator);
        return letters;
    }

    std::vector<std::string> texts(const std::string& text) {
        std::vector<std::string> result;
        for (const Token& token : tokenizer.tokenize(text)) {
            result.push_back(text_of(text, token));
        }
        return result;
    }
};

TEST_F(TokenizerTest, WordsWithPositions) {
    std::string text = "hello, world!";
    std::vector<Token> tokens = tokenizer.tokenize(text);
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(text_of(text, tokens[0]), "hello");
    EXPECT_EQ(tokens[0].start, 0u);
    EXPECT_EQ(tokens[0].end, 5u);
    EXPECT_EQ(text_of(text, tokens[1]), "world");
    EXPECT_EQ(tokens[1].start, 7u);
    EXPECT_EQ(tokens[1].end, 12u);
}

TEST_F(TokenizerTest, NonLatinWordsWithBytePositions) {
    std::string text = "привет мир";
    std::vector<Token> tokens = tokenizer.tokenize(text);
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(text_of(text, tokens[0]), "привет");
    EXPECT_EQ(tokens[0].start, 0u);
    EXPECT_EQ(tokens[0].end, 12u);
    EXPECT_EQ(text_of(text, tokens[1]), "мир");
    EXPECT_EQ(tokens[1].start, 13u);
    EXPECT_EQ(tokens[1].end, 19u);
}

TEST_F(TokenizerTest, MixedScriptWordIsOneToken) {
    EXPECT_EQ(texts("wopты"), (std::vector<std::string>{"wopты"}));
}

TEST_F(TokenizerTest, PunctuationHyphensAndUnderscoresSplitWords) {
    EXPECT_EQ(texts("-fuck- _shit_ (ass) arse-licking"), (std::vector<std::string>{"fuck", "shit", "ass", "arse", "licking"}));
}

TEST_F(TokenizerTest, DigitsStayInsideWords) {
    EXPECT_EQ(texts("c0ck 2010 a55"), (std::vector<std::string>{"c0ck", "2010", "a55"}));
}

TEST_F(TokenizerTest, EmptyAndSeparatorsOnly) {
    EXPECT_TRUE(tokenizer.tokenize("").empty());
    EXPECT_TRUE(tokenizer.tokenize(" ,.!\n").empty());
}

TEST_F(TokenizerTest, BrokenUtf8SplitsWords) {
    EXPECT_EQ(texts("ab\xFF" "cd"), (std::vector<std::string>{"ab", "cd"}));
}

TEST_F(TokenizerTest, SpacedLettersJoined) {
    for (const std::string& text : {"f u c k", "f.u.c.k", "f-u-c-k", "f_u_c_k", "f*u*c*k"}) {
        std::vector<Token> tokens = tokenizer.tokenize(text);
        ASSERT_EQ(tokens.size(), 1u) << text;
        EXPECT_EQ(text_of(text, tokens[0]), "fuck") << text;
        EXPECT_EQ(tokens[0].start, 0u) << text;
        EXPECT_EQ(tokens[0].end, 7u) << text;
    }
}

TEST_F(TokenizerTest, SpacedLettersInsideSentence) {
    std::string text = "you f u c k off";
    std::vector<Token> tokens = tokenizer.tokenize(text);
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(text_of(text, tokens[0]), "you");
    EXPECT_EQ(text_of(text, tokens[1]), "fuck");
    EXPECT_EQ(tokens[1].start, 4u);
    EXPECT_EQ(tokens[1].end, 11u);
    EXPECT_EQ(text_of(text, tokens[2]), "off");
}

TEST_F(TokenizerTest, SpacedCyrillicLettersJoined) {
    EXPECT_EQ(texts("с у к а"), (std::vector<std::string>{"сука"}));
}

TEST_F(TokenizerTest, TwoSpacedLettersNotJoined) {
    EXPECT_EQ(texts("a b"), (std::vector<std::string>{"a", "b"}));
}

TEST_F(TokenizerTest, MixedOrDoubleSeparatorsNotJoined) {
    EXPECT_EQ(texts("f.u-c k"), (std::vector<std::string>{"f", "u", "c", "k"}));
    EXPECT_EQ(texts("f  u  c  k"), (std::vector<std::string>{"f", "u", "c", "k"}));
}

TEST_F(TokenizerTest, SpacedLettersAcrossBatchBoundaryJoined) {
    std::string padding(65534, 'x');
    EXPECT_EQ(texts(padding + " f u c k you more"), (std::vector<std::string>{padding, "fuck", "you", "more"}));
}

TEST_F(TokenizerTest, JoinSeparatesWordsWithSingleSpaces) {
    std::string text = "arse-licking,  you f u c k";
    EXPECT_EQ(tokenizer.join(text, tokenizer.tokenize(text)), "arse licking you fuck");
    EXPECT_EQ(tokenizer.join("", tokenizer.tokenize("")), "");
}

TEST_F(TokenizerTest, SpacedDigitsAndLongerPiecesNotJoined) {
    EXPECT_EQ(texts("1 2 3"), (std::vector<std::string>{"1", "2", "3"}));
    EXPECT_EQ(texts("fu ck"), (std::vector<std::string>{"fu", "ck"}));
}

TEST_F(TokenizerTest, AsciiBlocksSplitAtClassEdges) {
    std::string text;
    std::vector<std::string> expected;
    for (int i = 0; i < 20; ++i) {
        text += "/09:@AZ[`az{ ";
        expected.insert(expected.end(), {"09", "AZ", "az"});
    }
    text += std::string(150, 'x') + " ";
    expected.push_back(std::string(150, 'x'));
    EXPECT_EQ(texts(text), expected);
}

TEST_F(TokenizerTest, LongNonAsciiTextStopsAtEnd) {
    std::string text;
    for (int i = 0; i < 1000; ++i) {
        text += "мир ";
    }
    std::size_t size = text.size();
    // Letters left past size() catch reads beyond the end
    text.append(8192, 'z');
    text.resize(size);
    EXPECT_EQ(texts(text), std::vector<std::string>(1000, "мир"));
}

TEST_F(TokenizerTest, NonAsciiLettersAndBrokenSequences) {
    EXPECT_EQ(texts("aµb"), (std::vector<std::string>{"aµb"}));
    EXPECT_EQ(texts(std::string("x\xC3" "Ay")), (std::vector<std::string>{"x", "Ay"}));
}
