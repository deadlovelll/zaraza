#include "pdf_page/pdf_page.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <utf8proc.h>

#include "epdf_edit.h"
#include "fpdf_edit.h"

const std::string& PdfPage::text() const {
    return utf8_text;
}

void PdfPage::read_text() {
    int char_count = FPDFText_CountChars(text_page.get());
    for (int index = 0; index < char_count; ++index) {
        unsigned int code_point = FPDFText_GetUnicode(text_page.get(), index);
        utf8proc_uint8_t bytes[4];
        utf8proc_ssize_t length = utf8proc_encode_char(static_cast<utf8proc_int32_t>(code_point), bytes);
        utf8_text.append(reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(length));
        char_of_byte.insert(char_of_byte.end(), static_cast<std::size_t>(length), index);
    }
}

// collects profane words from page with their positions
std::vector<PdfWord> PdfPage::words(const std::vector<SourceSpan>& spans) {
    std::vector<PdfWord> found;
    for (const SourceSpan& span : spans) {
        PdfWord word = make_word(char_of_byte[span.start], char_of_byte[span.end - 1]);
        word.stars = span.stars;
        found.push_back(word);
    }
    return found;
}

void PdfPage::redact(const std::vector<PdfWord>& words) {
    // why do we separate the background? because `embedpdf` does not allow replacing text
    // located on a block without causing artifacts, if we don't isolate the background,
    // the replaced text will appear inside a white rectangle. Therefore, when we encounter
    // profane words positioned on blocks, we temporarily remove those blocks and then add them back.
    std::vector<std::pair<FPDF_PAGEOBJECT, std::size_t>> detached = detach_background(words);
    FPDF_BOOL recurse_forms = true;
    FPDF_BOOL draw_black_boxes = false;
    bool redacted = false;
    for (const PdfWord& word : words) {
        if (EPDFText_RedactInRect(page.get(), &word.box, recurse_forms, draw_black_boxes)) {
            redacted = true;
        }
    }
    restore_background(detached);
    if (!redacted) {
        throw std::runtime_error("censor_file: the text could not be removed from a PDF page");
    }
}

void PdfPage::draw_text(
    const PdfWord& word,
    const char censor_char
) {
    double unit_squared = word.matrix.a * word.matrix.a + word.matrix.b * word.matrix.b;
    if (unit_squared == 0) {
        return;
    }
    std::vector<FPDF_WCHAR> stars(word.stars, static_cast<unsigned char>(censor_char));
    stars.push_back(0);

    // censor chars will always be in helvetica because it is supported by every pdf viewer
    // if we use the font of the text from pdf, there is a chance that we will get empty space or a box instead of the star
    FPDF_PAGEOBJECT object = FPDFPageObj_NewTextObj(document, "Helvetica", word.font_size);
    FPDFText_SetText(object, stars.data());
    FPDFPageObj_SetFillColor(object, word.red, word.green, word.blue, word.alpha);

    // get width for the censored version of profane word 
    float right = 0;
    float left = 0;
    // this ones we didn't need, but it's necessary for the function call
    float bottom = 0;
    float top = 0;
    //
    FPDFPageObj_GetBounds(object, &left, &bottom, &right, &top);

    // we need to get the width and height of the original profane word
    double word_width = word.box.right - word.box.left;
    double word_height = word.box.top - word.box.bottom;
    double word_length = (std::abs(word.matrix.a) * word_width + std::abs(word.matrix.b) * word_height) / unit_squared;

    // at the first look it's clear that one star per letter is enough, but letters and stars have
    // different widths (`i` is narrow, `w` is wide), so the stars would be shorter or longer than the word
    // thats why we need upscale the censored version to the original one
    double stars_width = right - left;
    double scale = 1.0;
    if (stars_width > 0) {
        scale = word_length / stars_width;
    }
    
    FPDFPageObj_Transform(
        object,
        scale * word.matrix.a,
        scale * word.matrix.b,
        word.matrix.c,
        word.matrix.d,
        word.x,
        word.y
    );
    FPDFPage_InsertObject(page.get(), object);
}

void PdfPage::commit() {
    if (!FPDFPage_GenerateContent(page.get())) {
        throw std::runtime_error("censor_file: a censored PDF page could not be written");
    }
}

FPDF_PAGE PdfPage::load(
    FPDF_DOCUMENT document,
    int index
) {
    FPDF_PAGE loaded = FPDF_LoadPage(document, index);
    if (loaded == nullptr) {
        throw std::invalid_argument("censor_file: a PDF page could not be loaded");
    }
    return loaded;
}

// scrape profane word from the document with their position, font, color etc
PdfWord PdfPage::make_word(
    int first,
    int last
) {
    PdfWord word{};
    std::size_t char_box_count = 0;
    for (int index = first; index <= last; ++index) {
        double left = 0;
        double right = 0;
        double bottom = 0;
        double top = 0;

        // for information: when we get text from pdf (pdfium is used here)
        // it can put some generated chars. so with this func call we skip them
        if (FPDFText_IsGenerated(text_page.get(), index) == 1) {
            continue;
        }
        // each char in the pdf has a rectangle, we need to get it by their coords
        // if we cant get rectangle, we skip char, otherwise word rectangle goes to page corner
        if (FPDFText_GetCharBox(text_page.get(), index, &left, &right, &bottom, &top)) {
            FS_RECTF char_box;
            char_box.left = static_cast<float>(left);
            char_box.right = static_cast<float>(right);
            char_box.bottom = static_cast<float>(bottom);
            char_box.top = static_cast<float>(top);
            if (char_box_count == 0) {
                word.box = char_box;
            } else {
                word.box.left = std::min(word.box.left, char_box.left);
                word.box.right = std::max(word.box.right, char_box.right);
                word.box.bottom = std::min(word.box.bottom, char_box.bottom);
                word.box.top = std::max(word.box.top, char_box.top);
            }
            ++char_box_count;
        }
    }
    if (char_box_count == 0) {
        throw std::runtime_error("censor_file: a matched word has no position on the PDF page");
    }
    // get x,y coordinates for censor char insertion
    FPDFText_GetCharOrigin(text_page.get(), first, &word.x, &word.y);
    FPDFText_GetMatrix(text_page.get(), first, &word.matrix);
    
    word.font_size = static_cast<float>(FPDFText_GetFontSize(text_page.get(), first));
    bool colored = FPDFText_GetFillColor(
        text_page.get(),
        first,
        &word.red,
        &word.green,
        &word.blue,
        &word.alpha
    );
    if (!colored) {
        word.red = 0;
        word.green = 0;
        word.blue = 0;
        word.alpha = 255;
    }
    return word;
}

// removes any background for profane words in pdf
std::vector<std::pair<FPDF_PAGEOBJECT, std::size_t>> PdfPage::detach_background(
    const std::vector<PdfWord>& words
) {
    std::vector<std::pair<FPDF_PAGEOBJECT, std::size_t>> detached;
    int count = FPDFPage_CountObjects(page.get());
    for (int index = 0; index < count; ++index) {
        FPDF_PAGEOBJECT object = FPDFPage_GetObject(page.get(), index);
        int type = FPDFPageObj_GetType(object);
        // https://github.com/embedpdf/runtime/blob/1461b0c5105137954b77dea0a6e2c15668518651/public/fpdf_edit.h#L38-L43
        // these types of objects can't contain any words, so skip all the others
        if (
            type != FPDF_PAGEOBJ_PATH
            && type != FPDF_PAGEOBJ_IMAGE
            && type != FPDF_PAGEOBJ_SHADING
        ) {
            continue;
        }
        // if text is overlapped by any object (e.g color block) store it
        if (overlaps_words(object, words)) {
            detached.emplace_back(object, static_cast<std::size_t>(index));
        }
    }
    // then remove those blocks, later we will recover them
    for (const std::pair<FPDF_PAGEOBJECT, std::size_t>& object : detached) {
        FPDFPage_RemoveObject(page.get(), object.first);
    }
    return detached;
}

bool PdfPage::overlaps_words(
    FPDF_PAGEOBJECT object,
    const std::vector<PdfWord>& words
) {
    float left = 0;
    float bottom = 0;
    float right = 0;
    float top = 0;
    if (!FPDFPageObj_GetBounds(object, &left, &bottom, &right, &top)) {
        return false;
    }
    for (const PdfWord& word : words) {
        // check if the element's border overlap with the word's border, 
        // even if just at the edge
        if (
            // if an element begins to the left of the end of a word 
            // and ends to the right of the beginning of that word, 
            // it means they overlap along the x-axis.
            left < word.box.right && right > word.box.left
            && 
            // same for y-axis
            bottom < word.box.top && top > word.box.bottom
        ) {
            return true;
        }
    }
    return false;
}

void PdfPage::restore_background(const std::vector<std::pair<FPDF_PAGEOBJECT, std::size_t>>& detached) {
    for (const std::pair<FPDF_PAGEOBJECT, std::size_t>& object : detached) {
        FPDFPage_InsertObjectAtIndex(page.get(), object.first, object.second);
    }
}
