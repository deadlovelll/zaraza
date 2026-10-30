#include <cstddef>
#include <exception>
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
#include "python_errors/python_errors.hpp"

namespace py = pybind11;

PYBIND11_MODULE(zaraza, z) {
    z.doc() = R"doc(Find and censor profanity in text and files.

Example:
    >>> detector = ProfaneDetector()
    >>> detector.censor("you fuck off")
    'you **** ***'
)doc";
    py::class_<ProfaneDetector>(
        z,
        "ProfaneDetector",
        R"doc(Find and censor profanity with a word list.

Every hidden character becomes *. Censoring releases the GIL.
)doc"
    )
        .def(
            py::init([](const std::string& lang, const std::string& custom_dictionary, bool ambiguous, bool deobfuscate) {
                std::filesystem::path module_path = py::module_::import("zaraza").attr("__file__").cast<std::filesystem::path>();
                std::filesystem::path data = module_path.parent_path() / "faster_profanity_data" / lang;
                std::string substitutions = deobfuscate ? (data / "substitutions.txt").string() : "";
                if (!custom_dictionary.empty()) {
                    if (ambiguous) {
                        throw py::value_error("ambiguous works with the bundled word list, not with custom_dictionary");
                    }
                    return std::make_unique<ProfaneDetector>(lang, custom_dictionary, "", deobfuscate, "", "", substitutions);
                }
                std::string ambiguous_dictionary = ambiguous ? (data / "ambiguous_wordslist.txt").string() : "";
                std::string variant_dictionary = deobfuscate ? (data / "variant_wordslist.txt").string() : "";
                std::string glue_dictionary = deobfuscate ? (data / "glue_wordslist.txt").string() : "";
                return std::make_unique<ProfaneDetector>(
                    lang, (data / "profanity_wordslist.txt").string(), ambiguous_dictionary, deobfuscate, variant_dictionary,
                    glue_dictionary, substitutions
                );
            }),
            py::arg("lang") = "en",
            py::arg("custom_dictionary") = "",
            py::arg("ambiguous") = false,
            py::arg("deobfuscate") = false,
            R"doc(Load the word list.

Args:
    lang: Language of the bundled word list.
    custom_dictionary: Path to your own word list, one word or
        phrase per line. When set, it replaces the bundled list and
        lang is ignored.
    ambiguous: Also hide words that are often harmless, such as
        "dick" in "Moby Dick". Off by default.
    deobfuscate: Also find disguised spellings such as sh1t, f*ck,
        a$$hole, fuuuck, phuck, fцck, f u c k, fu ck and fuckyou. Off by
        default: it makes the search about three times as slow.

Raises:
    RuntimeError: The word list cannot be opened.
    ValueError: ambiguous is set together with custom_dictionary.
)doc"
        )
        .def(
            "censor",
            [](ProfaneDetector& detector, const py::str& text) -> py::object {
                Py_ssize_t size = 0;
                const char* data = PyUnicode_AsUTF8AndSize(text.ptr(), &size);
                if (data == nullptr) {
                    throw py::error_already_set();
                }
                std::string_view view(data, static_cast<std::size_t>(size));
                std::string censored;
                bool changed = false;
                {
                    py::gil_scoped_release release;
                    changed = detector.censor(view, censored);
                }
                if (!changed) {
                    return text;
                }
                return py::str(censored);
            },
            py::arg("text"),
            R"doc(Return the text with every profane word hidden.

Args:
    text: Text to censor.

Returns:
    The censored text, or text itself, without a copy, when nothing
    was found.
)doc"
        )
        .def(
            "censor_inplace",
            [](ProfaneDetector& detector, const py::buffer& buffer) {
                std::size_t size = 0;
                {
                    py::buffer_info info = buffer.request(true);
                    if (info.ndim != 1 || info.itemsize != 1 || info.strides[0] != 1) {
                        throw py::value_error("censor_inplace needs a contiguous buffer of bytes");
                    }
                    std::span<char> text(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size));
                    py::gil_scoped_release release;
                    size = detector.censor_inplace(text);
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
            R"doc(Hide profane words in a buffer of UTF-8 bytes.

The buffer is edited in place, without a copy: bytearray, memoryview,
mmap or any other writable contiguous buffer of bytes can be used.

Args:
    buffer: UTF-8 text to censor.

Returns:
    The length of the censored text in bytes.

Raises:
    BufferError: The buffer is read-only, such as bytes.
    ValueError: The buffer is not a contiguous buffer of bytes.
)doc"
        )
        .def(
            "censor_file",
            [](ProfaneDetector& detector, const std::filesystem::path& path) {
                try {
                    py::gil_scoped_release release;
                    return detector.censor_file(path);
                } catch (const std::system_error& error) {
                    PythonErrors python_errors;
                    python_errors.set_os_error(error, path);
                    throw py::error_already_set();
                }
            },
            py::arg("path"),
            R"doc(Censor a file in place.

Accepts UTF-8 and UTF-16 text, DOCX, XLSX, PPTX and PDF files.

Args:
    path: Path to the file.

Returns:
    The size of the file after censoring, in bytes.

Raises:
    OSError: The file cannot be read or written.
    ValueError: The format is not supported, or the PDF is broken or
        password-protected.
)doc"
        )
        .def(
            "censor_files",
            [](ProfaneDetector& detector, const std::vector<std::filesystem::path>& paths) {
                std::vector<std::exception_ptr> errors;
                {
                    py::gil_scoped_release release;
                    errors = detector.censor_files(paths);
                }
                PythonErrors python_errors;
                py::list result(errors.size());
                for (std::size_t index = 0; index < errors.size(); ++index) {
                    if (errors[index]) {
                        result[index] = python_errors.from_exception(errors[index], paths[index]);
                    } else {
                        result[index] = py::none();
                    }
                }
                return result;
            },
            py::arg("paths"),
            R"doc(Censor several files in place, spread over the interop threads.

Each file is censored like in censor_file. A file listed twice is
censored once. A failed file does not stop the others.

Args:
    paths: Paths to the files.

Returns:
    A list with one item per path: None when the file was censored,
    or the exception censor_file would have raised for it.
)doc"
        )
        .def(
            "contains_profanity",
            &ProfaneDetector::contains_profanity,
            py::call_guard<py::gil_scoped_release>(),
            py::arg("text"),
            R"doc(Return True if the text contains a profane word.

The search stops at the first match, so it is cheaper than censor
when only the answer is needed.

Args:
    text: Text to check.
)doc"
        )
        .def(
            "censor_batch",
            [](ProfaneDetector& detector, const py::list& texts) {
                py::tuple items(texts);
                std::vector<std::string_view> views;
                views.reserve(items.size());
                for (py::handle text : items) {
                    Py_ssize_t size = 0;
                    const char* data = PyUnicode_AsUTF8AndSize(text.ptr(), &size);
                    if (data == nullptr) {
                        throw py::error_already_set();
                    }
                    views.emplace_back(data, static_cast<std::size_t>(size));
                }
                std::vector<std::optional<std::string>> censored;
                {
                    py::gil_scoped_release release;
                    censored = detector.censor_batch(views);
                }
                py::list result(censored.size());
                for (std::size_t index = 0; index < censored.size(); ++index) {
                    if (censored[index]) {
                        result[index] = py::str(*censored[index]);
                    } else {
                        result[index] = items[index];
                    }
                }
                return result;
            },
            py::arg("texts"),
            R"doc(Censor a list of texts, spread over the interop threads.

Args:
    texts: List of str.

Returns:
    A list of censored texts in the same order. A text without
    profanity is the same object as in texts, without a copy.

Raises:
    TypeError: An item of texts is not a str.
)doc"
        )
        .def(
            "set_num_threads",
            &ProfaneDetector::set_num_threads,
            py::arg("threads"),
            R"doc(Set the number of threads that share one big text.

Args:
    threads: Number of threads.

Raises:
    ValueError: threads is 0.
    RuntimeError: A call has already used threads; after that both
        thread counts are fixed.
)doc"
        )
        .def(
            "get_num_threads",
            &ProfaneDetector::get_num_threads,
            R"doc(Return the number of threads that share one big text.)doc"
        )
        .def(
            "set_num_interop_threads",
            &ProfaneDetector::set_num_interop_threads,
            py::arg("threads"),
            R"doc(Set the number of threads for censor_batch and censor_files.

Args:
    threads: Number of threads.

Raises:
    ValueError: threads is 0.
    RuntimeError: A call has already used threads; after that both
        thread counts are fixed.
)doc"
        )
        .def(
            "get_num_interop_threads",
            &ProfaneDetector::get_num_interop_threads,
            R"doc(Return the number of threads for censor_batch and censor_files.)doc"
        );
}
