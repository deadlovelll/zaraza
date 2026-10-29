#pragma once

#include <cstddef>

#include "fpdfview.h"

struct PdfWord {
    FS_RECTF box;
    FS_MATRIX matrix;
    double x;
    double y;
    float font_size;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
    std::size_t stars;
};
