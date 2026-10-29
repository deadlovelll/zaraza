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
        tf::Executor* executor
    );
private:
    bool censor_page(
        PdfPage& page,
        ProfaneFinder& finder,
        tf::Executor* executor
    );
};
