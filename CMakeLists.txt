cmake_minimum_required(VERSION 3.20)

project(faster_profanity LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LTO: lets the compiler inline functions across .cpp files, including utf8proc
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/src/data DESTINATION ${CMAKE_BINARY_DIR}/)

add_library(simdutf STATIC
    third_party/simdutf/simdutf.cpp
)

target_include_directories(simdutf
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/simdutf
)

target_compile_definitions(simdutf
    PUBLIC
        SIMDUTF_FEATURE_DETECT_ENCODING=0
        SIMDUTF_FEATURE_ASCII=0
        SIMDUTF_FEATURE_LATIN1=0
        SIMDUTF_FEATURE_UTF32=0
        SIMDUTF_FEATURE_BASE64=0
)

add_library(miniz STATIC
    third_party/miniz/miniz.c
)

target_include_directories(miniz
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/miniz
)

target_compile_definitions(miniz
    PUBLIC
        MINIZ_NO_ZLIB_COMPATIBLE_NAMES
)

add_library(libdeflate STATIC
    third_party/libdeflate/lib/arm/cpu_features.c
    third_party/libdeflate/lib/x86/cpu_features.c
    third_party/libdeflate/lib/crc32.c
    third_party/libdeflate/lib/deflate_compress.c
    third_party/libdeflate/lib/deflate_decompress.c
    third_party/libdeflate/lib/utils.c
)

target_include_directories(libdeflate
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/libdeflate
)

add_library(pugixml STATIC
    third_party/pugixml/pugixml.cpp
)

target_include_directories(pugixml
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/pugixml
)

find_package(Threads REQUIRED)

add_library(taskflow INTERFACE)

target_include_directories(taskflow
    INTERFACE
        ${PROJECT_SOURCE_DIR}/third_party/taskflow
)

target_link_libraries(taskflow
    INTERFACE
        Threads::Threads
)

add_library(faster_profanity_lib
    src/profane_detector/profane_detector.cpp
    src/profane_finder/profane_finder.cpp
    src/file_censor/file_censor.cpp
    src/office_censor/office_censor.cpp
    src/office_archive/office_archive.cpp
    src/utf16_converter/utf16_converter.cpp
    src/swear_word_hider/swear_word_hider.cpp
    src/censor_words_loader/censor_words_loader.cpp
    src/trie_loader/trie_loader.cpp
    src/word_filter/word_filter.cpp
    src/tokenizer/tokenizer.cpp
    src/unicode_normalizer/unicode_normalizer.cpp
    src/diacritics_stripper/diacritics_stripper.cpp
    src/char_classifier/char_classifier.cpp
    src/confusables_mapper/confusables_mapper.cpp
)

target_include_directories(faster_profanity_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/mio
)

if(WIN32)
    target_compile_definitions(faster_profanity_lib PRIVATE NOMINMAX)
endif()

target_link_libraries(faster_profanity_lib
    PUBLIC
        taskflow
    PRIVATE
        utf8proc
        simdutf
        miniz
        libdeflate
        pugixml
)

add_executable(faster_profanity
    src/main.cpp
)

target_link_libraries(faster_profanity
    PRIVATE
        faster_profanity_lib
)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v3.1.0
)

FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG        v2.12.0
)

set(PYBIND11_FINDPYTHON NEW CACHE STRING "")
FetchContent_MakeAvailable(pybind11)
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(utf8proc)

pybind11_add_module(zaraza
    src/py_main.cpp
)

target_link_libraries(zaraza
    PRIVATE
        faster_profanity_lib
)

enable_testing()
add_subdirectory(tests)