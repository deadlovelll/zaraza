#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive_part/archive_part.hpp"
#include "libdeflate.h"
#include "mio.hpp"
#include "miniz.h"

class OfficeArchive {
public:
    explicit OfficeArchive(const std::filesystem::path& path) : path(path), file(path.c_str()) {
        if (!mz_zip_reader_init_mem(&reader, file.data(), file.size(), 0)) {
            throw std::invalid_argument(broken_archive);
        }
        parts.resize(mz_zip_reader_get_num_files(&reader));
        decompressor = libdeflate_alloc_decompressor();
        compressor = libdeflate_alloc_compressor(6);
    }
    ~OfficeArchive() {
        close();
        libdeflate_free_decompressor(decompressor);
        libdeflate_free_compressor(compressor);
    }
    OfficeArchive(const OfficeArchive&) = delete;
    OfficeArchive& operator=(const OfficeArchive&) = delete;

    bool read(const std::string& name, std::string& content);
    void replace(const std::string& name, const std::string& content);
    std::size_t save();
private:
    const char* const broken_archive = "censor_file: broken or unsupported ZIP archive";

    std::filesystem::path path;
    mio::mmap_source file;
    mz_zip_archive reader{};
    libdeflate_decompressor* decompressor = nullptr;
    libdeflate_compressor* compressor = nullptr;
    std::vector<ArchivePart> parts;
    bool changed = false;

    void reject_forged_size(const mz_zip_archive_file_stat& stat);
    void reject_low_disk_space(const std::filesystem::path& target);
    bool find(const std::string& name, mz_zip_archive_file_stat& stat);
    bool inflate(const mz_zip_archive_file_stat& stat, std::string& content);
    bool extract(const mz_zip_archive_file_stat& stat, std::string& content);
    ArchivePart deflate(const char* name, const std::string& content);
    void close();
    std::size_t write_archive(const std::filesystem::path& temp);
    bool write_part(mz_zip_archive& writer, mz_uint index);
};
