cmake_minimum_required(VERSION 3.18)

if(DEFINED PROJECT_NAME)
    message(FATAL_ERROR
        "QtRpc project root folder should not be added as a CMake sub-project.\n"
        "Consider adding the subfolder ./QtRpc from this project."
        )
endif()

project("QtRpc" VERSION 1.0 LANGUAGES CXX)

# Project standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060000)
if(NOT MSVC)
    add_compile_options(-Werror -Wall -Wextra -Wpedantic -Wdeprecated-declarations)
endif()

# Choose Qt version
if(NOT DEFINED QT_PACKAGE)
    set(QT_PACKAGE "Qt6")
endif()

# Conan CMake support
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.16.1/conan.cmake"
        "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

# Install requirements from Conan
include(conan)
conan_cmake_configure(
    REQUIRES msgpack-cxx
    GENERATORS cmake_find_package
    )
conan_cmake_autodetect(settings)
conan_cmake_install(
    PATH_OR_REFERENCE .
    BUILD missing
    REMOTE conancenter
    SETTINGS ${settings}
    )

# Fetch QtPromise
include(FetchContent)
FetchContent_Declare(qtpromise
  GIT_REPOSITORY https://github.com/pwuertz/qtpromise.git
  GIT_TAG        pw/qt6
  GIT_SHALLOW    true
)
FetchContent_MakeAvailable(qtpromise)

add_subdirectory("QtRpc")
add_subdirectory("tests")
