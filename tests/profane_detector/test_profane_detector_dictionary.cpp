#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "fpdf_text.h"
#include "fpdfview.h"
#include "miniz.h"
#include "pdf_thread/pdf_thread.hpp"
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

static void write_zip(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& entries,
    mz_uint level
) {
    mz_zip_archive zip{};
    mz_zip_writer_init_heap(&zip, 0, 0);
    for (const std::pair<std::string, std::string>& entry : entries) {
        mz_zip_writer_add_mem(&zip, entry.first.c_str(), entry.second.data(), entry.second.size(), level);
    }
    void* data = nullptr;
    std::size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &data, &size);
    std::ofstream(path, std::ios::binary).write(static_cast<char*>(data), static_cast<std::streamsize>(size));
    mz_free(data);
    mz_zip_writer_end(&zip);
}

static std::string read_zip_entry(const std::filesystem::path& path, const std::string& name) {
    std::string archive = read_file(path);
    mz_zip_archive zip{};
    mz_zip_reader_init_mem(&zip, archive.data(), archive.size(), 0);
    std::size_t size = 0;
    char* data = static_cast<char*>(mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &size, 0));
    std::string entry = data == nullptr ? "" : std::string(data, size);
    mz_free(data);
    mz_zip_reader_end(&zip);
    return entry;
}

static const std::string office_types =
    R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
    R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
    R"(<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>)"
    R"(<Override PartName="/word/header1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml"/>)"
    R"(<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>)"
    R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
    R"(<Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>)"
    R"(<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>)"
    R"(</Types>)";

TEST(ProfaneDetectorDictionaryTest, NonAsciiDictionaryWordFound) {
    ProfaneDetector detector = make_detector("сука\n");
    for (const std::string& text : {"Сука", "СУКА", "сука"}) {
        std::string censored = detector.censor(text);
        EXPECT_EQ(censored.find_first_not_of('*'), std::string::npos) << text << " -> " << censored;
    }
}

TEST(ProfaneDetectorDictionaryTest, SpacedLettersFound) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_EQ(detector.censor("you f u c k off"), "you ******* off");
    EXPECT_EQ(detector.censor("f.u.c.k"), "*******");
}

TEST(ProfaneDetectorDictionaryTest, AmbiguousWordsNeedTheirList) {
    std::filesystem::path folder = std::filesystem::temp_directory_path();
    std::filesystem::path main_list = folder / "ambiguous_test_main.txt";
    std::filesystem::path ambiguous_list = folder / "ambiguous_test_extra.txt";
    std::ofstream(main_list) << "fuck\n";
    std::ofstream(ambiguous_list) << "dick\n";
    ProfaneDetector plain("", main_list.string());
    ProfaneDetector strict("", main_list.string(), ambiguous_list.string());
    EXPECT_EQ(plain.censor("Moby Dick, fuck"), "Moby Dick, ****");
    EXPECT_EQ(strict.censor("Moby Dick, fuck"), "Moby ****, ****");
    std::filesystem::remove(main_list);
    std::filesystem::remove(ambiguous_list);
}

TEST(ProfaneDetectorDictionaryTest, PhraseAcrossBatchBoundaryFound) {
    ProfaneDetector detector = make_detector("son of a bitch\n");
    std::string padding(65530, 'x');
    EXPECT_EQ(detector.censor(padding + " son of a bitch"), padding + " *** ** * *****");
    EXPECT_TRUE(detector.contains_profanity(padding + " son of a bitch"));
}

TEST(ProfaneDetectorDictionaryTest, WordsBetweenHyphensAndUnderscoresFound) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_EQ(detector.censor("-fuck- _fuck_"), "-****- _****_");
}

TEST(ProfaneDetectorDictionaryTest, LatinPartOfNonLatinWordNotFound) {
    ProfaneDetector detector = make_detector("wop\n");
    EXPECT_EQ(detector.censor("шорты").find('*'), std::string::npos);
}

TEST(ProfaneDetectorDictionaryTest, PhraseAcrossPunctuationAndNewlinesFound) {
    ProfaneDetector detector = make_detector("alabama hot pocket\n");
    EXPECT_EQ(detector.censor("alabama,  hot\npocket"), "*******,  ***\n******");
}

TEST(ProfaneDetectorDictionaryTest, HyphenatedDictionaryEntryFound) {
    ProfaneDetector detector = make_detector("arse-licking\n");
    EXPECT_EQ(detector.censor("arse licking"), "**** *******");
    EXPECT_EQ(detector.censor("arse-licking"), "****-*******");
}

TEST(ProfaneDetectorDictionaryTest, CensorKeepsOriginalText) {
    ProfaneDetector detector = make_detector("fuck\nсука\n");
    EXPECT_EQ(detector.censor("Привет, как дела? FUCK you, СУКА!"), "Привет, как дела? **** you, ****!");
}

TEST(ProfaneDetectorDictionaryTest, InvisibleCharsAndMarksInsideWordMasked) {
    ProfaneDetector detector = make_detector("fuck\n");
    EXPECT_EQ(detector.censor("fu​ck f̶u̶c̶k̶!"), "**** ****!");
    EXPECT_EQ(detector.censor("fu\u0E48ck"), "****");
}

TEST(ProfaneDetectorDictionaryTest, OneCensorCharPerOriginalCharacter) {
    ProfaneDetector detector = make_detector("ass\noff\n");
    EXPECT_EQ(detector.censor("aß"), "**");
    EXPECT_EQ(detector.censor("o\uFB00"), "**");
}

TEST(ProfaneDetectorDictionaryTest, ThreeAndFourByteLettersFound) {
    ProfaneDetector detector = make_detector("漢字\n𠜎𠜱\n");
    EXPECT_EQ(detector.censor("漢字 𠜎𠜱 漢𠜎"), "** ** 漢𠜎");
}

TEST(ProfaneDetectorDictionaryTest, DigitsInDictionaryWordFound) {
    ProfaneDetector detector = make_detector("a55hole\n");
    EXPECT_EQ(detector.censor("you A55HOLE"), "you *******");
    EXPECT_EQ(detector.censor("asshole a55"), "asshole a55");
}

TEST(ProfaneDetectorDictionaryTest, NonAsciiWordAfterFailLinkFound) {
    ProfaneDetector detector = make_detector("ab cd ef\ncd жук\n");
    EXPECT_EQ(detector.censor("ab cd жук"), "ab ** ***");
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
        EXPECT_EQ(detector.censor(padding + "s o n of a bitch" + tail), padding + "***** ** * *****" + tail) << offset;
        EXPECT_EQ(detector.censor(padding + "son of a bitch" + tail), padding + "*** ** * *****" + tail) << offset;
        EXPECT_EQ(detector.censor(padding + "ก่ f u c k сука fuck" + tail), padding + "** ******* **** ****" + tail) << offset;
    }
}

TEST(ProfaneDetectorDictionaryTest, SkippedWordBreaksPhrase) {
    ProfaneDetector detector = make_detector("hot\npocket\nhot pocket\n");
    EXPECT_EQ(detector.censor("hot dog pocket"), "*** dog ******");
    EXPECT_EQ(detector.censor("hot pocket"), "*** ******");
    std::string tail;
    while (tail.size() < 200000) {
        tail += " ab";
    }
    for (std::size_t offset = 65520; offset < 65540; ++offset) {
        std::string padding(offset % 3, 'x');
        while (padding.size() < offset) {
            padding += "ab ";
        }
        EXPECT_EQ(detector.censor(padding + "hot dog pocket" + tail), padding + "*** dog ******" + tail) << offset;
    }
}

TEST(ProfaneDetectorDictionaryTest, PhraseAcrossTokenBlocksFound) {
    ProfaneDetector detector = make_detector("son of a bitch\nfuck off\n");
    for (std::size_t count = 56; count < 72; ++count) {
        std::string padding;
        for (std::size_t i = 0; i < count; ++i) {
            padding += "ab ";
        }
        EXPECT_EQ(detector.censor(padding + "son of a bitch ab"), padding + "*** ** * ***** ab") << count;
        EXPECT_EQ(detector.censor(padding + "f.u.c.k off ab"), padding + "******* *** ab") << count;
    }
}

TEST(ProfaneDetectorDictionaryTest, LongDictionaryWordsFound) {
    ProfaneDetector detector = make_detector("motherfucker\nmotherfuckingasshole\nfat motherfucking bastard\n");
    EXPECT_EQ(detector.censor("motherfucker"), "************");
    EXPECT_EQ(detector.censor("you motherfucker!"), "you ************!");
    EXPECT_EQ(detector.censor("a motherfuckingasshole"), "a ********************");
    EXPECT_EQ(detector.censor("motherfuckingasshole ok"), "******************** ok");
    EXPECT_EQ(detector.censor("so fat motherfucking bastard ok"), "so *** ************* ******* ok");
    EXPECT_EQ(detector.censor("motherfuckingarsehole motherfuckers"), "motherfuckingarsehole motherfuckers");
}

TEST(ProfaneDetectorDictionaryTest, WordsAfterManyStreamingBatchesFound) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::string text;
    std::string censored;
    while (text.size() < 1000000) {
        text += "café fuck ";
        censored += "café **** ";
    }
    EXPECT_EQ(detector.censor(text), censored);
}

TEST(ProfaneDetectorDictionaryTest, TokensLongerThanBatchKeptWhole) {
    ProfaneDetector detector = make_detector("fuck\n");
    for (std::size_t length : {65536u, 131072u, 262144u, 300000u}) {
        std::string token(length, 'x');
        EXPECT_EQ(detector.censor(token + "fuck fuck"), token + "fuck ****") << length;
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
    EXPECT_FALSE(detector.censor("hello there", censored));
    EXPECT_EQ(censored, "untouched");
    EXPECT_TRUE(detector.censor("hello fuck", censored));
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
        std::size_t size = detector.censor_inplace(buffer);
        EXPECT_EQ(buffer.substr(0, size), detector.censor(text)) << text.substr(0, 40);
    }
}

TEST(ProfaneDetectorDictionaryTest, TokenEndingInsideExpansionHidesWholeCharacter) {
    ProfaneDetector detector = make_detector("xa1\na m\n");
    std::string buffer = "xa\u2488";
    EXPECT_EQ(detector.censor(buffer), "***");
    std::size_t size = detector.censor_inplace(buffer);
    EXPECT_EQ(buffer.substr(0, size), "***");
    // \u33c2 -> a.m. gives both tokens of the phrase
    EXPECT_EQ(detector.censor("x \u33c2 y"), "x * y");
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
        std::size_t size = detector.censor_file(path);
        EXPECT_EQ(read_file(path), detector.censor(text)) << text.substr(0, 20);
        EXPECT_EQ(size, std::filesystem::file_size(path));
    }
    std::filesystem::remove(path);
}

static std::string utf16_bytes(const std::u16string& text, bool big_endian) {
    std::string bytes = big_endian ? "\xFE\xFF" : "\xFF\xFE";
    bytes.reserve(bytes.size() + 2 * text.size());
    for (char16_t unit : text) {
        char high = static_cast<char>(unit >> 8);
        char low = static_cast<char>(unit & 0xFF);
        bytes.push_back(big_endian ? high : low);
        bytes.push_back(big_endian ? low : high);
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
        {u"сука\u2488", u"*****"},
        {u"𠜎𠜱 ok", u"** ok"},
        {u"start" + tail, u"start" + censored_tail},
    };
    for (bool big_endian : {false, true}) {
        for (const std::pair<std::u16string, std::u16string>& item : cases) {
            std::ofstream(path, std::ios::binary) << utf16_bytes(item.first, big_endian);
            std::size_t size = detector.censor_file(path);
            EXPECT_EQ(read_file(path), utf16_bytes(item.second, big_endian)) << big_endian << " " << item.first.size();
            EXPECT_EQ(size, std::filesystem::file_size(path));
        }
    }
    std::ofstream(path, std::ios::binary) << utf16_bytes(u"сука ok", false) + "x";
    detector.censor_file(path);
    EXPECT_EQ(read_file(path), utf16_bytes(u"**** ok", false) + "x");
    std::ofstream(path, std::ios::binary) << utf16_bytes(u"", false) + "x";
    detector.censor_file(path);
    EXPECT_EQ(read_file(path), utf16_bytes(u"", false) + "x");
    std::string lone = utf16_bytes(std::u16string(u"fuck ") + char16_t(0xD800) + u"a", false);
    std::ofstream(path, std::ios::binary) << lone;
    EXPECT_THROW(detector.censor_file(path), std::invalid_argument);
    EXPECT_EQ(read_file(path), lone);
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileSplitsLargeUtf16IntoChunks) {
    ProfaneDetector detector = make_detector("fuck\nсука\n𠜎𠜱\nson of a bitch\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_utf16_chunks_test.txt";
    std::u16string lines;
    std::u16string censored_lines;
    std::u16string phrases;
    std::u16string censored_phrases;
    std::u16string words;
    std::u16string censored_words;
    // longer than one 2 MB chunk, so the text is cut after a line break or a space
    while (phrases.size() < 1100000) {
        lines += u"ab сука fuck 𠜎𠜱\n";
        censored_lines += u"ab **** **** **\n";
        phrases += u"son of a bitch\n";
        censored_phrases += u"*** ** * *****\n";
        words += u"ab сука fuck 𠜎𠜱 ";
        censored_words += u"ab **** **** ** ";
    }
    // no line break or space in the first chunk, and its end falls inside a surrogate pair
    std::u16string pairs = u"x";
    while (pairs.size() < 1600000) {
        pairs += u"𠜎";
    }
    std::vector<std::pair<std::u16string, std::u16string>> cases = {
        {lines, censored_lines},
        {phrases, censored_phrases},
        {words, censored_words},
        {pairs + u" fuck", pairs + u" ****"},
    };
    for (std::size_t index = 0; index < cases.size(); ++index) {
        std::ofstream(path, std::ios::binary) << utf16_bytes(cases[index].first, false);
        std::size_t size = detector.censor_file(path);
        EXPECT_TRUE(read_file(path) == utf16_bytes(cases[index].second, false)) << "case " << index;
        EXPECT_EQ(size, std::filesystem::file_size(path)) << "case " << index;
    }
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, ParallelCensorMatchesSerial) {
    std::string words = "fuck\nсука\nson of a bitch\nass\n";
    ProfaneDetector detector = make_detector(words);
    std::string text;
    std::size_t line = 0;
    while (text.size() < 6000000) {
        text += "glass son of a\nbitch f u c k сука ";
        text += std::string(line % 97, 'x') + " fuck😀ass";
        text += line % 5 == 0 ? "\n" : " ";
        ++line;
    }
    std::string serial = detector.censor(text);
    for (std::size_t threads : {2, 3, 8}) {
        ProfaneDetector threaded = make_detector(words);
        threaded.set_num_threads(threads);
        EXPECT_EQ(threaded.get_num_threads(), threads);
        EXPECT_TRUE(threaded.censor(text) == serial) << threads;
        std::string buffer = text;
        std::size_t size = threaded.censor_inplace(buffer);
        EXPECT_TRUE(buffer.substr(0, size) == serial) << threads;
        EXPECT_EQ(threaded.censor("you fuck off"), "you **** off") << threads;
    }
    ProfaneDetector short_only = make_detector(words);
    short_only.set_num_threads(4);
    EXPECT_EQ(short_only.censor("you fuck off"), "you **** off");
    short_only.set_num_threads(2);
    detector.set_num_threads(8);
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_parallel_test.txt";
    std::ofstream(path, std::ios::binary) << text;
    detector.censor_file(path);
    EXPECT_TRUE(read_file(path) == serial);
    std::filesystem::remove(path);
    EXPECT_THROW(detector.set_num_threads(0), std::invalid_argument);
    EXPECT_THROW(detector.set_num_threads(2), std::runtime_error);
}

TEST(ProfaneDetectorDictionaryTest, CensorBatchMatchesCensor) {
    ProfaneDetector detector = make_detector("fuck\nсука\nson of a bitch\n");
    std::vector<std::string> texts;
    for (std::size_t i = 0; i < 2000; ++i) {
        texts.push_back("msg " + std::to_string(i) + (i % 3 == 0 ? " you fuck off" : " clean") + (i % 7 == 0 ? " сука" : ""));
    }
    std::string big;
    while (big.size() < 1000000) {
        big += "son of a\nbitch ok fuck ";
    }
    texts.push_back(big);
    texts.push_back("");
    std::vector<std::optional<std::string>> expected;
    for (const std::string& text : texts) {
        std::string censored;
        if (detector.censor(text, censored)) {
            expected.push_back(censored);
        } else {
            expected.push_back(std::nullopt);
        }
    }
    std::vector<std::string_view> views(texts.begin(), texts.end());
    EXPECT_TRUE(detector.censor_batch(views) == expected);
    detector.set_num_interop_threads(4);
    detector.set_num_threads(4);
    EXPECT_EQ(detector.get_num_interop_threads(), 4);
    EXPECT_TRUE(detector.censor_batch(views) == expected);
    EXPECT_THROW(detector.set_num_interop_threads(0), std::invalid_argument);
    EXPECT_THROW(detector.set_num_interop_threads(2), std::runtime_error);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileRejectsUnsupportedAndMissingFiles) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_unsupported_test.txt";
    for (std::string unsupported : {
        std::string("\xFF\xFE\0\0" "f\0\0\0", 8),
        std::string("\0\0\xFE\xFF" "\0\0\0f", 8),
        std::string("PK\x03\x04 fuck"),
        std::string("%PDF-1.7 fuck"),
        std::string("\xD0\xCF\x11\xE0 fuck"),
    }) {
        std::ofstream(path, std::ios::binary) << unsupported;
        EXPECT_THROW(detector.censor_file(path), std::invalid_argument);
        EXPECT_EQ(read_file(path), unsupported);
    }
    write_zip(path, {{"a.txt", "fuck"}}, MZ_DEFAULT_LEVEL);
    std::string plain_zip = read_file(path);
    EXPECT_THROW(detector.censor_file(path), std::invalid_argument);
    EXPECT_EQ(read_file(path), plain_zip);
    write_zip(path, {{"[Content_Types].xml", office_types}, {"word/document.xml", "<w:p><w:r><w:t>fuck</w:t></w:r></w:p>"}}, MZ_DEFAULT_LEVEL);
    std::string wrong_crc = read_file(path);
    wrong_crc[wrong_crc.rfind("PK\x01\x02") + 16] ^= 1;
    std::ofstream(path, std::ios::binary) << wrong_crc;
    EXPECT_THROW(detector.censor_file(path), std::invalid_argument);
    EXPECT_EQ(read_file(path), wrong_crc);
    std::filesystem::remove(path);
    EXPECT_THROW(detector.censor_file(path), std::system_error);
}

static std::string make_pdf(const std::string& content) {
    std::vector<std::string> objects = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
        "<< /Length " + std::to_string(content.size()) + " >>\nstream\n" + content + "\nendstream",
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    };
    std::string pdf = "%PDF-1.7\n";
    std::vector<std::size_t> offsets;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        offsets.push_back(pdf.size());
        pdf += std::to_string(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    std::size_t xref = pdf.size();
    pdf += "xref\n0 " + std::to_string(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (std::size_t offset : offsets) {
        std::string number = std::to_string(offset);
        pdf += std::string(10 - number.size(), '0') + number + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size " + std::to_string(objects.size() + 1) + " /Root 1 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
    return pdf;
}

TEST(ProfaneDetectorDictionaryTest, CensorFileHidesPdfWordsAndKeepsBackground) {
    ProfaneDetector detector = make_detector("fucking\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_pdf_test.pdf";
    std::ofstream(path, std::ios::binary) << make_pdf("q 0.13 0.42 0.82 rg 72 640 468 80 re f Q BT /F1 30 Tf 0 0 0 rg 96 668 Td (You fucking idiot!) Tj ET");
    detector.censor_file(path);
    std::string censored = read_file(path);
    detector.censor_file(path);
    EXPECT_EQ(read_file(path), censored);
    PdfThread thread;
    FPDF_DOCUMENT document = FPDF_LoadDocument(path.string().c_str(), nullptr);
    FPDF_PAGE page = FPDF_LoadPage(document, 0);
    FPDF_TEXTPAGE text_page = FPDFText_LoadPage(page);
    std::string text;
    for (int index = 0; index < FPDFText_CountChars(text_page); ++index) {
        text += static_cast<char>(FPDFText_GetUnicode(text_page, index));
    }
    EXPECT_EQ(text.find("fucking"), std::string::npos);
    EXPECT_NE(text.find("*******"), std::string::npos);
    FPDF_BITMAP bitmap = FPDFBitmap_Create(612, 792, 0);
    FPDFBitmap_FillRect(bitmap, 0, 0, 612, 792, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, 612, 792, 0, 0);
    const unsigned char* pixels = static_cast<const unsigned char*>(FPDFBitmap_GetBuffer(bitmap));
    int white = 0;
    // the banner covers y 640..720 pt, at 72 dpi one pixel per point
    for (int y = 792 - 720; y < 792 - 640; ++y) {
        for (int x = 72; x < 540; ++x) {
            const unsigned char* pixel = pixels + y * FPDFBitmap_GetStride(bitmap) + x * 4;
            white += pixel[0] > 240 && pixel[1] > 240 && pixel[2] > 240;
        }
    }
    EXPECT_EQ(white, 0);
    FPDFBitmap_Destroy(bitmap);
    FPDFText_ClosePage(text_page);
    FPDF_ClosePage(page);
    FPDF_CloseDocument(document);
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileSizesPdfStarsByTextMatrix) {
    ProfaneDetector detector = make_detector("fucking\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_pdf_matrix_test.pdf";
    // font size 1 scaled to 30 by the text matrix, the way macOS writes PDFs
    std::ofstream(path, std::ios::binary) << make_pdf("BT /F1 1 Tf 30 0 0 30 96 668 Tm (You fucking idiot!) Tj ET");
    detector.censor_file(path);
    PdfThread thread;
    FPDF_DOCUMENT document = FPDF_LoadDocument(path.string().c_str(), nullptr);
    FPDF_PAGE page = FPDF_LoadPage(document, 0);
    FPDF_TEXTPAGE text_page = FPDFText_LoadPage(page);
    double star_height = 0;
    for (int index = 0; index < FPDFText_CountChars(text_page); ++index) {
        double left = 0;
        double right = 0;
        double bottom = 0;
        double top = 0;
        if (FPDFText_GetUnicode(text_page, index) != '*') {
            continue;
        }
        if (FPDFText_GetCharBox(text_page, index, &left, &right, &bottom, &top)) {
            star_height = top - bottom;
            break;
        }
    }
    EXPECT_GT(star_height, 5.0);
    FPDFText_ClosePage(text_page);
    FPDF_ClosePage(page);
    FPDF_CloseDocument(document);
    std::filesystem::remove(path);
}


TEST(ProfaneDetectorDictionaryTest, CensorFilesReportsEveryFile) {
    ProfaneDetector detector = make_detector("fuck\n");
    detector.set_num_interop_threads(4);
    std::filesystem::path folder = std::filesystem::temp_directory_path();
    std::filesystem::path text = folder / "censor_files_test.txt";
    std::filesystem::path pdf = folder / "censor_files_test.pdf";
    std::filesystem::path missing = folder / "censor_files_missing.txt";
    std::ofstream(text, std::ios::binary) << "you fuck off";
    std::ofstream(pdf, std::ios::binary) << make_pdf("BT /F1 30 Tf 96 668 Td (you fuck off) Tj ET");
    std::filesystem::remove(missing);
    std::vector<std::exception_ptr> errors = detector.censor_files({text, pdf, missing, folder / "." / "censor_files_test.txt"});
    ASSERT_EQ(errors.size(), 4);
    EXPECT_FALSE(errors[0]);
    EXPECT_FALSE(errors[1]);
    EXPECT_THROW(std::rethrow_exception(errors[2]), std::system_error);
    EXPECT_FALSE(errors[3]);
    EXPECT_EQ(read_file(text), "you **** off");
    std::filesystem::remove(text);
    std::filesystem::remove(pdf);
}
TEST(ProfaneDetectorDictionaryTest, CensorFileEditsOfficeText) {
    ProfaneDetector detector = make_detector("fuck\nсука\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_office_test.docx";
    std::string image("\x89PNG fuck \0\xFF", 12);
    std::string properties = "<Properties><Company>fuck</Company></Properties>";
    write_zip(path, {
        {"[Content_Types].xml", office_types},
        {"word/document.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n<w:document xmlns:w=\"w\"><w:body>"
            "<w:p><w:r><w:rPr><w:b/></w:rPr><w:t>you fu</w:t></w:r><w:r><w:t xml:space=\"preserve\">ck off</w:t></w:r></w:p>"
            "<w:p><w:r><w:t>fu</w:t></w:r><w:r><w:tab/><w:t>ck</w:t></w:r><w:r><w:t xml:space=\"preserve\"> </w:t></w:r></w:p>"
            "</w:body></w:document>"},
        {"word/header1.xml", "<w:hdr xmlns:w=\"w\"><w:p><w:r><w:t xml:space=\"preserve\">сука </w:t></w:r></w:p></w:hdr>"},
        {"xl/sharedStrings.xml", "<sst><si><t>fu</t></si><si><r><t>ck</t></r></si><si><r><t>fu</t></r><r><t>ck</t></r></si></sst>"},
        {"xl/worksheets/sheet1.xml", "<worksheet><sheetData><row><c t=\"inlineStr\"><is><t>fuck</t></is></c></row></sheetData></worksheet>"},
        {"ppt/slides/slide1.xml", "<p:sld xmlns:p=\"p\" xmlns:a=\"a\"><a:p><a:r><a:t>сука</a:t></a:r></a:p></p:sld>"},
        {"word/media/image1.png", image},
        {"docProps/app.xml", properties},
    }, MZ_DEFAULT_LEVEL);
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    std::filesystem::perms permissions = std::filesystem::status(path).permissions();
    std::size_t size = detector.censor_file(path);
    EXPECT_EQ(size, std::filesystem::file_size(path));
    EXPECT_EQ(std::filesystem::status(path).permissions(), permissions);
    std::string document = read_zip_entry(path, "word/document.xml");
    EXPECT_NE(document.find("<w:t>you **</w:t></w:r><w:r><w:t xml:space=\"preserve\">** off</w:t>"), std::string::npos) << document;
    EXPECT_NE(document.find("<w:t>fu</w:t></w:r><w:r><w:tab/><w:t>ck</w:t></w:r><w:r><w:t xml:space=\"preserve\"> </w:t>"), std::string::npos) << document;
    EXPECT_EQ(read_zip_entry(path, "word/header1.xml"), "<w:hdr xmlns:w=\"w\"><w:p><w:r><w:t xml:space=\"preserve\">**** </w:t></w:r></w:p></w:hdr>");
    EXPECT_EQ(read_zip_entry(path, "xl/sharedStrings.xml"), "<sst><si><t>fu</t></si><si><r><t>ck</t></r></si><si><r><t>**</t></r><r><t>**</t></r></si></sst>");
    EXPECT_EQ(read_zip_entry(path, "xl/worksheets/sheet1.xml"), "<worksheet><sheetData><row><c t=\"inlineStr\"><is><t>****</t></is></c></row></sheetData></worksheet>");
    EXPECT_EQ(read_zip_entry(path, "ppt/slides/slide1.xml"), "<p:sld xmlns:p=\"p\" xmlns:a=\"a\"><a:p><a:r><a:t>****</a:t></a:r></a:p></p:sld>");
    EXPECT_EQ(read_zip_entry(path, "word/media/image1.png"), image);
    EXPECT_EQ(read_zip_entry(path, "docProps/app.xml"), properties);
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileEditsStoredOfficeParts) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_stored_test.docx";
    write_zip(path, {{"[Content_Types].xml", office_types}, {"word/document.xml", "<w:p><w:r><w:t>fuck</w:t></w:r></w:p>"}}, MZ_NO_COMPRESSION);
    EXPECT_EQ(detector.censor_file(path), std::filesystem::file_size(path));
    EXPECT_EQ(read_zip_entry(path, "word/document.xml"), "<w:p><w:r><w:t>****</w:t></w:r></w:p>");
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileHandlesDeeplyNestedXml) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_deep_test.docx";
    std::string xml = "<w:p>";
    for (int depth = 0; depth < 100000; ++depth) {
        xml += "<x>";
    }
    xml += "<w:t>fuck</w:t>";
    for (int depth = 0; depth < 100000; ++depth) {
        xml += "</x>";
    }
    xml += "</w:p>";
    write_zip(path, {{"[Content_Types].xml", office_types}, {"word/document.xml", xml}}, MZ_DEFAULT_LEVEL);
    detector.censor_file(path);
    EXPECT_NE(read_zip_entry(path, "word/document.xml").find("<w:t>****</w:t>"), std::string::npos);
    std::filesystem::remove(path);
}

TEST(ProfaneDetectorDictionaryTest, CensorFileLeavesCleanOfficeFilesUntouched) {
    ProfaneDetector detector = make_detector("fuck\n");
    std::filesystem::path path = std::filesystem::temp_directory_path() / "censor_file_clean_test.docx";
    write_zip(path, {{"[Content_Types].xml", office_types}, {"word/document.xml", "<w:p><w:r><w:t>hello</w:t></w:r></w:p>"}}, MZ_DEFAULT_LEVEL);
    std::string original = read_file(path);
    std::filesystem::file_time_type modified = std::filesystem::last_write_time(path) - std::chrono::hours(1);
    std::filesystem::last_write_time(path, modified);
    EXPECT_EQ(detector.censor_file(path), original.size());
    EXPECT_EQ(read_file(path), original);
    EXPECT_EQ(std::filesystem::last_write_time(path), modified);
    std::filesystem::remove(path);
}
