#pragma once

#include <cstddef>
#include <filesystem>

#include "pdf_page/pdf_page.hpp"
#include "profane_finder/profane_finder.hpp"
#include "taskflow/taskflow.hpp"

class PdfCensor {
public:
    std::size_t censor(
        ProfaneFinder& finder,
        const std::filesystem::path& path,
        const char censor_char,
        tf::Executor* executor
    );
private:
    bool censor_page(
        PdfPage& page,
        ProfaneFinder& finder,
        const char censor_char,
        tf::Executor* executor
    );
};
