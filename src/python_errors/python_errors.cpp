#include "python_errors/python_errors.hpp"

#include <cerrno>
#include <stdexcept>

#include <pybind11/stl/filesystem.h>

void PythonErrors::set_os_error(
    const std::system_error& error,
    const std::filesystem::path& path
) {
    pybind11::object filename = pybind11::cast(path);
#ifdef _WIN32
    if (error.code().category() == std::system_category()) {
        PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_OSError, error.code().value(), filename.ptr());
        return;
    }
#endif
    errno = error.code().value();
    PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, filename.ptr());
}

pybind11::object PythonErrors::from_exception(
    const std::exception_ptr& error,
    const std::filesystem::path& path
) {
    try {
        std::rethrow_exception(error);
    } catch (const std::system_error& system_error) {
        set_os_error(system_error, path);
    } catch (const std::invalid_argument& invalid_argument) {
        PyErr_SetString(PyExc_ValueError, invalid_argument.what());
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "censor_files: unknown error");
    }
    return pybind11::error_already_set().value();
}
