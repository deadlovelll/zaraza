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
        || head.starts_with("%PDF")
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
    const char censor_char
) {
    std::size_t length = static_cast<std::size_t>(std::filesystem::file_size(path));
    if (length == 0) {
        return 0;
    }
    mio::mmap_sink file(path.c_str());
    std::span<char> text(file.data(), file.size());
    if (is_office_archive(text)) {
        file.unmap();
        return censor_office(finder, path, censor_char);
    }
    std::size_t size = censor_text(finder, text, censor_char);
    file.unmap();
    if (size != length) {
        std::filesystem::resize_file(path, size);
    }
    return size;
}

std::size_t FileCensor::censor_office(
    ProfaneFinder& finder,
    const std::filesystem::path& path,
    const char censor_char
) {
    OfficeArchive archive(path);
    for (const std::string& name : office_censor.text_parts(archive)) {
        OfficeText text;
        if (!office_censor.load(archive, name, text)) {
            continue;
        }
        std::vector<SourceSpan> spans = finder.find_spans(text.text);
        if (!spans.empty()) {
            archive.replace(name, office_censor.hide(text, spans, censor_char));
        }
    }
    return archive.save();
}

std::size_t FileCensor::censor_text(
    ProfaneFinder& finder,
    std::span<char> text,
    const char censor_char
) {
    std::string_view head(text.data(), std::min<std::size_t>(text.size(), 4));
    reject_unsupported_format(head);
    if (head.starts_with("\xFF\xFE") || head.starts_with("\xFE\xFF")) {
        return 2 + censor_utf16(finder, text.subspan(2), head.starts_with("\xFE\xFF"), censor_char);
    }
    return finder.censor_inplace(text, censor_char);
}

std::size_t FileCensor::censor_utf16(
    ProfaneFinder& finder,
    std::span<char> text,
    bool big_endian,
    const char censor_char
) {
    std::string utf8 = utf16_converter.to_utf8(text, big_endian);
    utf8.resize(finder.censor_inplace(utf8, censor_char));
    return utf16_converter.from_utf8(utf8, text, big_endian);
}
