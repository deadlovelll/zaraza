cmake_minimum_required(VERSION 3.20)

project(faster_profanity LANGUAGES C CXX)

option(FASTER_PROFANITY_BUILD_TESTS "Build the unit tests with the vendored googletest" OFF)

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
file(COPY
    ${CMAKE_SOURCE_DIR}/src/data/en/profanity_words/profanity_wordslist.txt
    ${CMAKE_SOURCE_DIR}/src/data/en/profanity_words/ambiguous_wordslist.txt
    ${CMAKE_SOURCE_DIR}/src/data/en/profanity_words/variant_wordslist.txt
    ${CMAKE_SOURCE_DIR}/src/data/en/profanity_words/glue_wordslist.txt
    ${CMAKE_SOURCE_DIR}/src/data/en/profanity_words/substitutions.txt
    DESTINATION ${CMAKE_BINARY_DIR}/faster_profanity_data/en
)

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

add_library(utf8proc STATIC
    third_party/utf8proc/utf8proc.c
)

target_include_directories(utf8proc
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/utf8proc
)

target_compile_definitions(utf8proc
    PUBLIC
        UTF8PROC_STATIC
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

if(APPLE)
    set(EMBEDPDF_PLATFORM darwin)
    set(EMBEDPDF_LIBRARY lib/libembedpdf.dylib)
elseif(WIN32)
    set(EMBEDPDF_PLATFORM win32)
    set(EMBEDPDF_LIBRARY bin/embedpdf.dll)
elseif(EXISTS /lib/ld-musl-x86_64.so.1 OR EXISTS /lib/ld-musl-aarch64.so.1)
    set(EMBEDPDF_PLATFORM linuxmusl)
    set(EMBEDPDF_LIBRARY lib/libembedpdf.a)
else()
    set(EMBEDPDF_PLATFORM linux)
    set(EMBEDPDF_LIBRARY lib/libembedpdf.so)
endif()

# cross builds: macOS wheels for x86_64 are built on arm64, Windows ARM64 with MSVC
set(EMBEDPDF_ARCH ${CMAKE_SYSTEM_PROCESSOR})
if(CMAKE_OSX_ARCHITECTURES)
    set(EMBEDPDF_ARCH ${CMAKE_OSX_ARCHITECTURES})
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
    set(EMBEDPDF_ARCH ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID})
endif()

if(EMBEDPDF_ARCH MATCHES "^(arm64|aarch64|ARM64)$")
    string(APPEND EMBEDPDF_PLATFORM -arm64)
else()
    string(APPEND EMBEDPDF_PLATFORM -x64)
endif()

set(EMBEDPDF_NAME libembedpdf-runtime-${EMBEDPDF_PLATFORM}-1461b0c51.tar.gz)
file(STRINGS ${PROJECT_SOURCE_DIR}/third_party/embedpdf/SHA256SUMS EMBEDPDF_SUM REGEX " ${EMBEDPDF_NAME}$")
string(REGEX MATCH "^[0-9a-f]+" EMBEDPDF_SHA256 "${EMBEDPDF_SUM}")
if(NOT EMBEDPDF_SHA256)
    message(FATAL_ERROR "No EmbedPDF runtime for ${EMBEDPDF_PLATFORM} in third_party/embedpdf/SHA256SUMS")
endif()

set(EMBEDPDF_ARCHIVE_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Folder with the EmbedPDF runtime archive; it is downloaded there when missing")
set(EMBEDPDF_ARCHIVE ${EMBEDPDF_ARCHIVE_DIR}/${EMBEDPDF_NAME})
if(NOT EXISTS ${EMBEDPDF_ARCHIVE})
    file(DOWNLOAD
        https://github.com/embedpdf/runtime/releases/download/runtime-1461b0c5105137954b77dea0a6e2c15668518651/${EMBEDPDF_NAME}
        ${EMBEDPDF_ARCHIVE}.part
        TLS_VERIFY ON
        STATUS EMBEDPDF_DOWNLOAD
    )
    file(SHA256 ${EMBEDPDF_ARCHIVE}.part EMBEDPDF_DOWNLOAD_SHA256)
    if(NOT EMBEDPDF_DOWNLOAD_SHA256 STREQUAL EMBEDPDF_SHA256)
        message(FATAL_ERROR "Could not download ${EMBEDPDF_NAME} (${EMBEDPDF_DOWNLOAD}); put it into EMBEDPDF_ARCHIVE_DIR")
    endif()
    file(RENAME ${EMBEDPDF_ARCHIVE}.part ${EMBEDPDF_ARCHIVE})
endif()

file(SHA256 ${EMBEDPDF_ARCHIVE} EMBEDPDF_ARCHIVE_SHA256)
if(NOT EMBEDPDF_ARCHIVE_SHA256 STREQUAL EMBEDPDF_SHA256)
    message(FATAL_ERROR "${EMBEDPDF_ARCHIVE} does not match its sha256 in third_party/embedpdf/SHA256SUMS")
endif()

file(ARCHIVE_EXTRACT
    INPUT ${EMBEDPDF_ARCHIVE}
    DESTINATION ${CMAKE_BINARY_DIR}/embedpdf
)

if(EMBEDPDF_PLATFORM MATCHES "^linuxmusl")
    add_library(embedpdf STATIC IMPORTED)
    set_target_properties(embedpdf PROPERTIES INTERFACE_LINK_LIBRARIES Threads::Threads)
else()
    add_library(embedpdf SHARED IMPORTED)
endif()

set_target_properties(embedpdf
    PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/embedpdf/${EMBEDPDF_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}/embedpdf/include
)

if(WIN32)
    set_target_properties(embedpdf PROPERTIES IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/embedpdf/lib/embedpdf.dll.lib)
endif()

add_library(faster_profanity_lib
    src/profane_detector/profane_detector.cpp
    src/profane_finder/profane_finder.cpp
    src/file_censor/file_censor.cpp
    src/office_censor/office_censor.cpp
    src/office_archive/office_archive.cpp
    src/pdf_censor/pdf_censor.cpp
    src/pdf_closer/pdf_closer.cpp
    src/pdf_document/pdf_document.cpp
    src/pdf_page/pdf_page.cpp
    src/utf16_converter/utf16_converter.cpp
    src/swear_word_hider/swear_word_hider.cpp
    src/deobfuscator/deobfuscator.cpp
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
        embedpdf
    PRIVATE
        utf8proc
        simdutf
        miniz
        libdeflate
        pugixml
)

set(PYBIND11_FINDPYTHON NEW CACHE STRING "")
add_subdirectory(third_party/pybind11)

pybind11_add_module(zaraza
    src/py_main.cpp
    src/python_errors/python_errors.cpp
)

target_link_libraries(zaraza
    PRIVATE
        faster_profanity_lib
)

install(TARGETS zaraza
    LIBRARY DESTINATION .
)

if(NOT EMBEDPDF_PLATFORM MATCHES "^linuxmusl")
    install(FILES ${CMAKE_BINARY_DIR}/embedpdf/${EMBEDPDF_LIBRARY}
        DESTINATION .
    )
endif()

if(APPLE)
    set_target_properties(zaraza PROPERTIES INSTALL_RPATH @loader_path)
elseif(UNIX)
    set_target_properties(zaraza PROPERTIES INSTALL_RPATH "\$ORIGIN")
endif()

install(FILES
    src/data/en/profanity_words/profanity_wordslist.txt
    src/data/en/profanity_words/ambiguous_wordslist.txt
    src/data/en/profanity_words/variant_wordslist.txt
    src/data/en/profanity_words/glue_wordslist.txt
    src/data/en/profanity_words/substitutions.txt
    DESTINATION faster_profanity_data/en
)

if(FASTER_PROFANITY_BUILD_TESTS)
    set(BUILD_GMOCK OFF)
    set(INSTALL_GTEST OFF)
    set(gtest_force_shared_crt ON)
    add_subdirectory(third_party/googletest)
    enable_testing()
    add_subdirectory(tests)
endif()