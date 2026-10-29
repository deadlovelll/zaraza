#include "pdf_document/pdf_document.hpp"

#include <cerrno>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "epdf_save.h"
#include "fpdf_save.h"

int PdfDocument::page_count() {
    return FPDF_GetPageCount(document.get());
}

PdfPage PdfDocument::load_page(int index) {
    return PdfPage(document.get(), index);
}

// writes censored file then saves it instead of original one
std::size_t PdfDocument::save(const std::filesystem::path& path) {
    unsigned long size = 0;
    std::unique_ptr<char, PdfCloser> bytes(
        static_cast<char*>(EPDF_SaveDocumentToOwnedBuffer(
            document.get(),
            // this flag means that we write the new file
            // instead of writing to existing one
            FPDF_NO_INCREMENTAL,
            &size
        ))
    );
    if (bytes == nullptr) {
        throw std::runtime_error("censor_file: the censored PDF could not be saved");
    }
    // the source file must be closed before it is replaced
    document.reset();
    
    std::filesystem::path temporary = path;
    temporary += ".censored";
    std::ofstream out(temporary, std::ios::binary);
    if (!out.is_open()) {
        throw std::system_error(errno, std::generic_category(), "censor_file: cannot create a temporary file");
    }
    out.write(bytes.get(), static_cast<std::streamsize>(size));
    out.close();
    if (out.fail()) {
        int error_number = errno;
        std::filesystem::remove(temporary);
        throw std::system_error(error_number, std::generic_category(), "censor_file: cannot write the censored PDF");
    }
    std::filesystem::rename(temporary, path);
    return size;
}

FPDF_DOCUMENT PdfDocument::open(const std::filesystem::path& path) {
    std::u8string name = path.u8string();
    FPDF_DOCUMENT opened = FPDF_LoadDocument(reinterpret_cast<const char*>(name.c_str()), nullptr);
    if (opened == nullptr) {
        throw_load_error(errno);
    }
    return opened;
}

void PdfDocument::throw_load_error(int error_number) {
    unsigned long error = FPDF_GetLastError();
    switch (error) {
        case FPDF_ERR_FILE:
            throw std::system_error(error_number, std::generic_category(), "censor_file: cannot open the PDF file");
        case FPDF_ERR_FORMAT:
            throw std::invalid_argument("censor_file: the file is not a valid PDF");
        case FPDF_ERR_PASSWORD:
            throw std::invalid_argument("censor_file: password-protected PDFs are not supported");
        case FPDF_ERR_SECURITY:
            throw std::invalid_argument("censor_file: the PDF uses an unsupported encryption scheme");
        case FPDF_ERR_PAGE:
            throw std::invalid_argument("censor_file: the PDF has a broken page");
        default:
            throw std::runtime_error("censor_file: EmbedPDF could not open the PDF, error " + std::to_string(error));
    }
}
