#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>

#include "fpdfview.h"
#include "pdf_closer/pdf_closer.hpp"
#include "pdf_page/pdf_page.hpp"
#include "pdf_thread/pdf_thread.hpp"

class PdfDocument {
public:
    explicit PdfDocument(const std::filesystem::path& path) : document(open(path)) {}
    int page_count();
    PdfPage load_page(int index);
    std::size_t save(const std::filesystem::path& path);
private:
    PdfThread thread;
    std::unique_ptr<fpdf_document_t__, PdfCloser> document;

    FPDF_DOCUMENT open(const std::filesystem::path& path);
    void throw_load_error(int error_number);
};
