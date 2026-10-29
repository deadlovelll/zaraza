#include "pdf_closer/pdf_closer.hpp"

#include "epdf_save.h"
#include "fpdf_text.h"

void PdfCloser::operator()(FPDF_DOCUMENT document) const {
    FPDF_CloseDocument(document);
}

void PdfCloser::operator()(FPDF_PAGE page) const {
    FPDF_ClosePage(page);
}

void PdfCloser::operator()(FPDF_TEXTPAGE text_page) const {
    FPDFText_ClosePage(text_page);
}

void PdfCloser::operator()(char* buffer) const {
    EPDF_FreeBuffer(buffer);
}
