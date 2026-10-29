#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "office_censor/office_censor.hpp"
#include "pdf_censor/pdf_censor.hpp"
#include "taskflow/taskflow.hpp"
#include "utf16_converter/utf16_converter.hpp"

class ProfaneFinder;

class FileCensor {
public:
    std::size_t censor_file(
        ProfaneFinder& finder,
        const std::filesystem::path& path,
        tf::Executor* executor
    );
private:
    static constexpr std::size_t utf16_chunk_bytes = 2 << 20;

    OfficeCensor office_censor;
    PdfCensor pdf_censor;
    Utf16Converter utf16_converter;

    void reject_unsupported_format(std::string_view head);
    bool is_office_archive(std::span<const char> text);
    std::size_t censor_office(
        ProfaneFinder& finder,
        const std::filesystem::path& path,
        tf::Executor* executor
    );
    std::size_t censor_text(
        ProfaneFinder& finder,
        std::span<char> text,
        tf::Executor* executor
    );
    std::size_t censor_utf16(
        ProfaneFinder& finder,
        std::span<char> text,
        bool big_endian,
        tf::Executor* executor
    );
    void keep_whole_lines(std::string& utf8);
};
