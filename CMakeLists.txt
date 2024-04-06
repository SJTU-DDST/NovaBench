# version
cmake_minimum_required(VERSION 3.9)

# bad taste
project(bench VERSION 11.45.14 LANGUAGES C CXX)

# cxx11
set(CMAKE_CXX_STANDARD 11)

# exception
add_compile_options(-fexceptions)

# compiler option
set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# test some relating lib
include(CheckLibraryExists)
check_library_exists(crc32c crc32c_value "" HAVE_CRC32C)
check_library_exists(snappy snappy_compress "" HAVE_SNAPPY)

# test compiler flag
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)

# include my header and database header
include_directories(
    "${PROJECT_BINARY_DIR}/src/include"
    "${PROJECT_BINARY_DIR}/external/leveldb/include"
    "${PROJECT_BINARY_DIR}/external/rocksdb/include"
)