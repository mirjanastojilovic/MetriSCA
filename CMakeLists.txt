# ---------------------------------------
# Top-level MetriSCA CMake file
# ---------------------------------------

cmake_minimum_required(VERSION 3.10)

project(metrisca
    DESCRIPTION
        "MetriSCA"
    LANGUAGES
        CXX C
)

# ---------------------------------------
# Optional features available to the user
# ---------------------------------------


# ---------------------------------------
# In-tree builds are not permitted
# ---------------------------------------

if (PROJECT_SOURCE_DIR STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-tree builds are not permitted. To recover, delete "
          "'CMakeCache.txt', the 'CMakeFiles' directory and inform CMake about "
          "the source (-S) and build (-B) paths. For example to compile to a "
          "directory labeled 'build' using the Ninja generator, enter\n"
          "  $ rm -Rf CMakeCache.txt CMakeFiles\n"
          "  $ cmake -S . -B build -G Ninja\n"
          "  $ cmake --build build")
endif()

# ---------------------------------------

# Directory for build products
if (MSVC)
  set(METRISCA_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
  set(METRISCA_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

if (MSVC)
    # Parallel compile on MSVC
    add_compile_options(/MP)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Register the dependencies
add_subdirectory(vendor)

# Include global MetriSCA directories
include_directories(includes)

set(CMAKE_CXX_STANDARD 17)

# Register MetriSCA codebase
add_subdirectory(src)

# Register MetriSCA tools
add_subdirectory(tools)

# Set location for build products
set_target_properties(metrisca metriscacli
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${METRISCA_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${METRISCA_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${METRISCA_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${METRISCA_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${METRISCA_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${METRISCA_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${METRISCA_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${METRISCA_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${METRISCA_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY_RELNODEBINFO ${METRISCA_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELNODEBINFO ${METRISCA_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY_RELNODEBINFO ${METRISCA_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${METRISCA_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${METRISCA_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${METRISCA_BINARY_DIR}
)

message("${CMAKE_INSTALL_INCLUDEDIR}")

if (MSVC)
    # Default startup project in MSVC
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT metrisca)
endif()
