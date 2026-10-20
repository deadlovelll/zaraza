#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

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
            &ProfaneDetector::censor,
            py::arg("text"),
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