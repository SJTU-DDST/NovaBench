# version
cmake_minimum_required(VERSION 3.9)

# bad taste
project(bench VERSION 11.45.14 LANGUAGES C CXX)

# cxx11
set(CMAKE_CXX_STANDARD 17)

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
    "${CMAKE_SOURCE_DIR}/src/include"
    "${CMAKE_SOURCE_DIR}/external/leveldb/include"
    "${CMAKE_SOURCE_DIR}/external/rocksdb/include"
)

# set link path
link_directories(${CMAKE_SOURCE_DIR}/external/leveldb)
link_directories(${CMAKE_SOURCE_DIR}/external/rocksdb)

# all files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# main file
file(GLOB_RECURSE MAIN_SOURCES "src/bench/main.cpp")

# test files
file(GLOB_RECURSE TEST_SOURCES "src/test/*.cpp")

# delete test files and main file from all files
list(REMOVE_ITEM SOURCES ${TEST_SOURCES})
list(REMOVE_ITEM SOURCES ${MAIN_SOURCES})

# define library and files
add_library(MyDataBase)
target_sources(MyDataBase PRIVATE
    ${SOURCES}
)

# add other supporting libraries 
function(find_static_library LIB_NAME OUT)

    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    find_library(
            FOUND_${LIB_NAME}_STATIC
            ${LIB_NAME}
    )

    if (FOUND_${LIB_NAME}_STATIC)
        get_filename_component(ABS_FILE ${FOUND_${LIB_NAME}_STATIC} ABSOLUTE)
    else()
        message(SEND_ERROR "Unable to find library ${LIB_NAME}")
    endif()

    set(${OUT} ${ABS_FILE} PARENT_SCOPE)

endfunction()

find_static_library(gflags GFLAGS)
find_package(Threads REQUIRED)
target_link_libraries(MyDataBase Threads::Threads -lpthread)
target_link_libraries(MyDataBase ibverbs event fmt)

# add main
add_executable(NovaBench "src/bench/main.cpp")
target_link_libraries(NovaBench -lgflags MyDataBase leveldb rocksdb)

# add test
add_executable(configtest "src/test/configtest.cpp")
target_link_libraries(configtest -lgflags MyDataBase leveldb rocksdb)

add_executable(generator "src/test/generator")