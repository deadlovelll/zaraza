#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

class [[nodiscard]] OutputBuffer {
public:
    OutputBuffer(std::string& storage, std::size_t capacity)
        : storage(&storage), data(nullptr), used(0), capacity(capacity) {
        storage.resize(capacity);
        data = storage.data();
    }
    void push_back(char c) {
        if (used == capacity) [[unlikely]] {
            data = grow(*storage, used + 1);
            capacity = storage->size();
        }
        data[used] = c;
        ++used;
    }
    void append(const char* bytes, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            push_back(bytes[i]);
        }
    }
    std::size_t size() const {
        return used;
    }
    std::string_view view() const {
        return std::string_view(data, used);
    }
    void finish() {
        storage->resize(used);
    }
private:
    std::string* storage;
    char* data;
    std::size_t used;
    std::size_t capacity;

    static char* grow(std::string& storage, std::size_t needed) {
        storage.resize(std::max(storage.size() * 2, needed));
        return storage.data();
    }
};
