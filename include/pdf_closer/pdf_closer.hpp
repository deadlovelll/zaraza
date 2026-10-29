#pragma once

#include "fpdfview.h"

struct PdfCloser {
    void operator()(FPDF_DOCUMENT document) const;
    void operator()(FPDF_PAGE page) const;
    void operator()(FPDF_TEXTPAGE text_page) const;
    void operator()(char* buffer) const;
};
