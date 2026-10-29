#pragma once

#include "epdf_view.h"

class PdfThread {
public:
    PdfThread() {
        EPDF_InitThread();
    }
    PdfThread(const PdfThread&) = delete;
    PdfThread& operator=(const PdfThread&) = delete;
    ~PdfThread() {
        EPDF_ShutdownThread();
    }
};
