#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "profane_detector/profane_detector.hpp"

namespace py = pybind11;

PYBIND11_MODULE(zaraza, z) {
    z.doc() = R"doc(docs will be here)doc";
    py::class_<ProfaneDetector>(
        z,
        "ProfaneDetector",
        R"doc(docs will be here)doc"
    )
        .def(
            py::init<const std::string&, const std::string&>(),
            py::arg("lang") = "en",
            py::arg("custom_dictionary") = "",
            R"doc(docs will be here)doc"
        )
        .def(
            "censor",
            [](ProfaneDetector& detector, const py::str& text, char censor_char) -> py::object {
                Py_ssize_t size = 0;
                const char* data = PyUnicode_AsUTF8AndSize(text.ptr(), &size);
                if (data == nullptr) {
                    throw py::error_already_set();
                }
                std::string_view view(data, static_cast<std::size_t>(size));
                std::string censored;
                if (!detector.censor(view, censor_char, censored)) {
                    return text;
                }
                return py::str(censored);
            },
            py::arg("text"),
            py::arg("censor_char"),
            R"doc(docs will be here)doc"
        )
        .def(
            "censor_inplace",
            [](ProfaneDetector& detector, const py::buffer& buffer, char censor_char) {
                std::size_t size = 0;
                {
                    py::buffer_info info = buffer.request(true);
                    if (info.ndim != 1 || info.itemsize != 1 || info.strides[0] != 1) {
                        throw py::value_error("censor_inplace needs a contiguous buffer of bytes");
                    }
                    std::span<char> text(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size));
                    size = detector.censor_inplace(text, censor_char);
                }
                if (
                    PyByteArray_Check(buffer.ptr())
                    && PyByteArray_Resize(buffer.ptr(), static_cast<Py_ssize_t>(size)) != 0
                ) {
                    throw py::error_already_set();
                }
                return size;
            },
            py::arg("buffer"),
            py::arg("censor_char"),
            R"doc(docs will be here)doc"
        )
        .def(
            "censor_file",
            [](ProfaneDetector& detector, const std::filesystem::path& path, char censor_char) {
                try {
                    return detector.censor_file(path, censor_char);
                } catch (const std::system_error& error) {
                    py::object filename = py::cast(path);
#ifdef _WIN32
                    if (error.code().category() == std::system_category()) {
                        PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_OSError, error.code().value(), filename.ptr());
                        throw py::error_already_set();
                    }
#endif
                    errno = error.code().value();
                    PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, filename.ptr());
                    throw py::error_already_set();
                }
            },
            py::arg("path"),
            py::arg("censor_char"),
            R"doc(docs will be here)doc"
        )
        .def(
            "contains_profanity",
            &ProfaneDetector::contains_profanity,
            py::arg("text"),
            R"doc(docs will be here)doc"
        );
}
