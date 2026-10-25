#include "office_archive/office_archive.hpp"

#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <system_error>

// this function is needed for the cases if the archive size is compromised
// e.g if archive says that the weight is 4gb but actual size is 10mb we can't 
// allow this 
void OfficeArchive::reject_forged_size(const mz_zip_archive_file_stat& stat) {
    const std::uint64_t max_deflate_ratio = 1032;
    if (stat.m_uncomp_size / max_deflate_ratio > stat.m_comp_size) {
        throw std::invalid_argument(broken_archive);
    }
}

bool OfficeArchive::read(const std::string& name, std::string& content) {
    mz_zip_archive_file_stat stat;
    if (!find(name, stat)) {
        return false;
    }
    reject_forged_size(stat);
    content.assign(stat.m_uncomp_size, '\0');
    bool extracted = (
        // if archive is compressed via DEFLATE we would go to fast path with libdeflate
        // otherwise miniz
        stat.m_method == MZ_DEFLATED 
        ? inflate(stat, content) 
        : extract(stat, content)
    );
    if (!extracted) {
        throw std::invalid_argument(broken_archive);
    }
    return true;
}

void OfficeArchive::replace(const std::string& name, const std::string& content) {
    mz_zip_archive_file_stat stat;
    if (!find(name, stat)) {
        throw std::invalid_argument("censor_file: no such part in the archive: " + name);
    }
    parts[stat.m_file_index] = deflate(stat.m_filename, content);
    changed = true;
}

std::size_t OfficeArchive::save() {
    if (!changed) {
        return file.size();
    }
    std::filesystem::path target = std::filesystem::canonical(path);
    std::filesystem::path temp = target;
    temp += ".tmp";
    try {
        std::size_t size = write_archive(temp);
        close();
        std::filesystem::permissions(temp, std::filesystem::status(target).permissions());
        std::filesystem::rename(temp, target);
        return size;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        throw;
    }
}

bool OfficeArchive::find(const std::string& name, mz_zip_archive_file_stat& stat) {
    int index = mz_zip_reader_locate_file(&reader, name.c_str(), nullptr, 0);
    return index >= 0 && mz_zip_reader_file_stat(&reader, index, &stat);
}

bool OfficeArchive::inflate(const mz_zip_archive_file_stat& stat, std::string& content) {
    std::vector<char> deflated(stat.m_comp_size);
    bool copied = mz_zip_reader_extract_to_mem(
        &reader,
        stat.m_file_index,
        deflated.data(),
        deflated.size(),
        MZ_ZIP_FLAG_COMPRESSED_DATA
    );
    if (!copied) {
        return false;
    }
    // nullptr means output must be exactly content.size()
    libdeflate_result result = libdeflate_deflate_decompress(
        decompressor,
        deflated.data(),
        deflated.size(),
        content.data(),
        content.size(),
        nullptr
    );
    if (result != LIBDEFLATE_SUCCESS) {
        return false;
    }
    // checksum from the archive must match the unpacked data
    return libdeflate_crc32(0, content.data(), content.size()) == stat.m_crc32;
}

bool OfficeArchive::extract(const mz_zip_archive_file_stat& stat, std::string& content) {
    return mz_zip_reader_extract_to_mem(&reader, stat.m_file_index, content.data(), content.size(), 0);
}

// Reads data from the xml part of archieve and return ArchivePart
ArchivePart OfficeArchive::deflate(const char* name, const std::string& content) {
    ArchivePart part;
    part.name = name;
    part.size = content.size();
    part.crc = libdeflate_crc32(0, content.data(), content.size());
    part.deflated.resize(libdeflate_deflate_compress_bound(compressor, content.size()));
    part.deflated.resize(libdeflate_deflate_compress(compressor, content.data(), content.size(), part.deflated.data(), part.deflated.size()));
    return part;
}

void OfficeArchive::close() {
    mz_zip_reader_end(&reader);
    file.unmap();
}

std::size_t OfficeArchive::write_archive(const std::filesystem::path& temp) {
    mz_zip_archive writer{};
    std::u8string temp_name = temp.u8string();
    bool opened = mz_zip_writer_init_file_v2(
        &writer,
        reinterpret_cast<const char*>(temp_name.c_str()),
        0,
        0
    );
    if (!opened) {
        throw std::system_error(errno, std::generic_category(), "censor_file: cannot create a temporary file");
    }
    bool written = true;
    // write contents to new zip
    for (mz_uint index = 0; written && index < parts.size(); ++index) {
        written = write_part(writer, index);
    }
    // write the zip table of contents, skip it if a part failed
    written = written && mz_zip_writer_finalize_archive(&writer);
    std::size_t size = writer.m_archive_size;
    written = mz_zip_writer_end(&writer) && written;
    if (!written) {
        // get the error cause if occured
        std::string error = mz_zip_get_error_string(mz_zip_get_last_error(&writer));
        throw std::runtime_error("censor_file: cannot write the censored archive: " + error);
    }
    return size;
}

bool OfficeArchive::write_part(mz_zip_archive& writer, mz_uint index) {
    const ArchivePart& part = parts[index];
    if (part.deflated.empty()) {
        return mz_zip_writer_add_from_zip_reader(&writer, &reader, index);
    }
    return mz_zip_writer_add_mem_ex(
        &writer,
        part.name.c_str(),
        part.deflated.data(),
        part.deflated.size(),
        nullptr,
        0,
        MZ_ZIP_FLAG_COMPRESSED_DATA,
        part.size,
        part.crc
    );
}
