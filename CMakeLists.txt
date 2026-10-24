cmake_minimum_required(VERSION 3.20)

project(faster_profanity LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LTO: lets the compiler inline functions across .cpp files, including utf8proc
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/src/data DESTINATION ${CMAKE_BINARY_DIR}/)

add_library(faster_profanity_lib
    src/profane_detector/profane_detector.cpp
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
)

target_link_libraries(faster_profanity_lib
    PUBLIC
        pybind11::module
    PRIVATE
        utf8proc
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
    GIT_TAG        v2.11.1  
)

FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG        v2.12.0
)

FetchContent_MakeAvailable(pybind11)
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(utf8proc)

enable_testing()
add_subdirectory(tests)