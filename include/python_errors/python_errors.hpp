#pragma once

#include <exception>
#include <filesystem>
#include <system_error>

#include <pybind11/pybind11.h>

class PythonErrors {
public:
    // OSError with the file name, the way Python's own file functions report it
    void set_os_error(
        const std::system_error& error,
        const std::filesystem::path& path
    );
    pybind11::object from_exception(
        const std::exception_ptr& error,
        const std::filesystem::path& path
    );
};
