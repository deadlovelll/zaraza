#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ArchivePart {
    std::string name;
    std::vector<char> deflated;
    std::size_t size;
    std::uint32_t crc;
};
