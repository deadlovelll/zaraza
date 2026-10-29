#include "file_censor/file_censor.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "mio.hpp"
#include "office_archive/office_archive.hpp"
#include "office_text/office_text.hpp"
#include "profane_finder/profane_finder.hpp"

void FileCensor::reject_unsupported_format(std::string_view head) {
    if (
        // utf32 le bom
        head.starts_with(std::string_view("\xFF\xFE\0\0", 4))
        // utf32 be bom
        || head.starts_with(std::string_view("\0\0\xFE\xFF", 4))
        // .doc, .xls, .ppt
        || head.starts_with("\xD0\xCF\x11\xE0")
    ) {
        throw std::invalid_argument("censor_file: only UTF-8 and UTF-16 text files are supported");
    }
}

bool FileCensor::is_office_archive(std::span<const char> text) {
    return std::string_view(text.data(), text.size()).starts_with("PK\x03\x04");
}

std::size_t FileCensor::censor_file(
    ProfaneFinder& finder,
    const std::filesystem::path& path,
    const char censor_char,
    tf::Executor* executor
) {
    std::size_t length = static_cast<std::size_t>(std::filesystem::file_size(path));
    if (length == 0) {
        return 0;
    }
    mio::mmap_sink file(path.c_str());
    std::span<char> text(file.data(), file.size());
    if (is_office_archive(text)) {
        file.unmap();
        return censor_office(finder, path, censor_char, executor);
    }
    if (std::string_view(text.data(), text.size()).starts_with("%PDF")) {
        file.unmap();
        return pdf_censor.censor(finder, path, censor_char, executor);
    }
    std::size_t size = censor_text(finder, text, censor_char, executor);
    file.unmap();
    if (size != length) {
        std::filesystem::resize_file(path, size);
    }
    return size;
}

std::size_t FileCensor::censor_office(
    ProfaneFinder& finder,
    const std::filesystem::path& path,
    const char censor_char,
    tf::Executor* executor
) {
    OfficeArchive archive(path);
    for (const std::string& name : office_censor.text_parts(archive)) {
        OfficeText text;
        if (!office_censor.load(archive, name, text)) {
            continue;
        }
        std::vector<SourceSpan> spans = finder.find_spans(text.text, executor);
        if (!spans.empty()) {
            archive.replace(name, office_censor.hide(text, spans, censor_char));
        }
    }
    return archive.save();
}

std::size_t FileCensor::censor_text(
    ProfaneFinder& finder,
    std::span<char> text,
    const char censor_char,
    tf::Executor* executor
) {
    std::string_view head(text.data(), std::min<std::size_t>(text.size(), 4));
    reject_unsupported_format(head);
    if (head.starts_with("\xFF\xFE") || head.starts_with("\xFE\xFF")) {
        return 2 + censor_utf16(finder, text.subspan(2), head.starts_with("\xFE\xFF"), censor_char, executor);
    }
    return finder.censor_inplace(text, censor_char, executor);
}

std::size_t FileCensor::censor_utf16(
    ProfaneFinder& finder,
    std::span<char> text,
    bool big_endian,
    const char censor_char,
    tf::Executor* executor
) {
    utf16_converter.validate(text, big_endian);
    std::span<char> units = text.first(text.size() & ~std::size_t{1});
    if (units.empty()) {
        return text.size();
    }
    std::size_t read = 0;
    std::size_t written = 0;
    while (read < units.size()) {
        std::span<char> chunk = units.subspan(read, std::min(units.size() - read, utf16_chunk_bytes));
        bool last = read + chunk.size() == units.size();
        std::string utf8 = utf16_converter.to_utf8(chunk, big_endian);
        if (!last) {
            keep_whole_lines(utf8);
        }
        read += utf16_converter.utf16_size(utf8);
        utf8.resize(finder.censor_inplace(utf8, censor_char, executor));
        std::span<char> target = last ? text.subspan(written) : text.subspan(written, read - written);
        written += utf16_converter.from_utf8(utf8, target, big_endian);
    }
    return written;
}

void FileCensor::keep_whole_lines(std::string& utf8) {
    std::size_t cut = utf8.rfind('\n');
    if (cut == std::string::npos) {
        cut = utf8.rfind(' ');
    }
    if (cut != std::string::npos) {
        utf8.resize(cut + 1);
    }
}
