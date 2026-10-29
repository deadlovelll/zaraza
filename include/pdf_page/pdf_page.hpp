#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "fpdf_text.h"
#include "fpdfview.h"
#include "pdf_closer/pdf_closer.hpp"
#include "pdf_word/pdf_word.hpp"
#include "source_span/source_span.hpp"

class PdfPage {
public:
    PdfPage(
        FPDF_DOCUMENT document,
        int index
    ) : document(document), page(load(document, index)), text_page(FPDFText_LoadPage(page.get())) {
        read_text();
    }
    const std::string& text() const;
    std::vector<PdfWord> words(const std::vector<SourceSpan>& spans);
    void redact(const std::vector<PdfWord>& words);
    void draw_text(
        const PdfWord& word,
        const char censor_char
    );
    void commit();
private:
    FPDF_DOCUMENT document;
    std::unique_ptr<fpdf_page_t__, PdfCloser> page;
    std::unique_ptr<fpdf_textpage_t__, PdfCloser> text_page;
    std::string utf8_text;
    std::vector<int> char_of_byte;

    void read_text();
    FPDF_PAGE load(
        FPDF_DOCUMENT document,
        int index
    );
    PdfWord make_word(
        int first,
        int last
    );
    std::vector<std::pair<FPDF_PAGEOBJECT, std::size_t>> detach_background(
        const std::vector<PdfWord>& words
    );
    bool overlaps_words(
        FPDF_PAGEOBJECT object,
        const std::vector<PdfWord>& words
    );
    void restore_background(const std::vector<std::pair<FPDF_PAGEOBJECT, std::size_t>>& detached);
};
