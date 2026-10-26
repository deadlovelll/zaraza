#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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
            py::init([](const std::string& lang, const std::string& custom_dictionary) {
                if (!custom_dictionary.empty()) {
                    return std::make_unique<ProfaneDetector>(lang, custom_dictionary);
                }
                std::filesystem::path module_path = py::module_::import("zaraza").attr("__file__").cast<std::filesystem::path>();
                std::filesystem::path dictionary = module_path.parent_path() / "faster_profanity_data" / lang / "profanity_wordslist.txt";
                return std::make_unique<ProfaneDetector>(lang, dictionary.string());
            }),
            py::arg("lang") = "en",
            py::arg("custom_dictionary") = "",
            R"doc(docs will be here)doc"
        )
        .def(
            "censor",
            [](ProfaneDetector& detector, const py::str& text, char censor_char, bool parallel) -> py::object {
                Py_ssize_t size = 0;
                const char* data = PyUnicode_AsUTF8AndSize(text.ptr(), &size);
                if (data == nullptr) {
                    throw py::error_already_set();
                }
                std::string_view view(data, static_cast<std::size_t>(size));
                std::string censored;
                if (!detector.censor(view, censor_char, censored, parallel)) {
                    return text;
                }
                return py::str(censored);
            },
            py::arg("text"),
            py::arg("censor_char"),
            py::arg("parallel") = false,
            R"doc(docs will be here)doc"
        )
        .def(
            "censor_inplace",
            [](ProfaneDetector& detector, const py::buffer& buffer, char censor_char, bool parallel) {
                std::size_t size = 0;
                {
                    py::buffer_info info = buffer.request(true);
                    if (info.ndim != 1 || info.itemsize != 1 || info.strides[0] != 1) {
                        throw py::value_error("censor_inplace needs a contiguous buffer of bytes");
                    }
                    std::span<char> text(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size));
                    size = detector.censor_inplace(text, censor_char, parallel);
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
            py::arg("parallel") = false,
            R"doc(docs will be here)doc"
        )
        .def(
            "censor_file",
            [](ProfaneDetector& detector, const std::filesystem::path& path, char censor_char, bool parallel) {
                try {
                    return detector.censor_file(path, censor_char, parallel);
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
            py::arg("parallel") = false,
            R"doc(docs will be here)doc"
        )
        .def(
            "contains_profanity",
            &ProfaneDetector::contains_profanity,
            py::arg("text"),
            R"doc(docs will be here)doc"
        )
        .def(
            "censor_batch",
            [](ProfaneDetector& detector, const py::list& texts, char censor_char, bool parallel) {
                std::vector<std::string_view> views;
                views.reserve(texts.size());
                for (py::handle text : texts) {
                    Py_ssize_t size = 0;
                    const char* data = PyUnicode_AsUTF8AndSize(text.ptr(), &size);
                    if (data == nullptr) {
                        throw py::error_already_set();
                    }
                    views.emplace_back(data, static_cast<std::size_t>(size));
                }
                std::vector<std::optional<std::string>> censored = detector.censor_batch(views, censor_char, parallel);
                py::list result(censored.size());
                for (std::size_t index = 0; index < censored.size(); ++index) {
                    if (censored[index]) {
                        result[index] = py::str(*censored[index]);
                    } else {
                        result[index] = texts[index];
                    }
                }
                return result;
            },
            py::arg("texts"),
            py::arg("censor_char"),
            py::arg("parallel") = false,
            R"doc(docs will be here)doc"
        )
        .def(
            "set_num_threads",
            &ProfaneDetector::set_num_threads,
            py::arg("threads"),
            R"doc(docs will be here)doc"
        )
        .def(
            "get_num_threads",
            &ProfaneDetector::get_num_threads,
            R"doc(docs will be here)doc"
        )
        .def(
            "set_num_interop_threads",
            &ProfaneDetector::set_num_interop_threads,
            py::arg("threads"),
            R"doc(docs will be here)doc"
        )
        .def(
            "get_num_interop_threads",
            &ProfaneDetector::get_num_interop_threads,
            R"doc(docs will be here)doc"
        );
}
