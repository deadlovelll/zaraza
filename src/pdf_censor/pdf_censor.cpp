#include "pdf_censor/pdf_censor.hpp"

#include <vector>

#include "pdf_document/pdf_document.hpp"
#include "pdf_word/pdf_word.hpp"
#include "source_span/source_span.hpp"

std::size_t PdfCensor::censor(
    ProfaneFinder& finder,
    const std::filesystem::path& path,
    const char censor_char,
    tf::Executor* executor
) {
    PdfDocument document(path);
    int page_count = document.page_count();
    bool changed = false;
    for (int index = 0; index < page_count; ++index) {
        PdfPage page = document.load_page(index);
        if (censor_page(page, finder, censor_char, executor)) {
            changed = true;
        }
    }
    if (!changed) {
        return static_cast<std::size_t>(std::filesystem::file_size(path));
    }
    return document.save(path);
}

bool PdfCensor::censor_page(
    PdfPage& page,
    ProfaneFinder& finder,
    const char censor_char,
    tf::Executor* executor
) {
    // get profane words with their positions in the text
    std::vector<SourceSpan> spans = finder.find_spans(page.text(), executor);
    if (spans.empty()) {
        return false;
    }
    // get profane words with their positions in the pdf file
    std::vector<PdfWord> words = page.words(spans);
    page.redact(words);
    for (const PdfWord& word : words) {
        page.draw_text(word, censor_char);
    }
    page.commit();
    return true;
}
