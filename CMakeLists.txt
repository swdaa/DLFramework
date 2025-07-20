cmake_minimum_required(VERSION 3.16)

cmake_minimum_required(VERSION 3.16)
project(block_scholes)

find_package(benchmark REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Armadillo REQUIRED)
find_package(glog REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(GTest REQUIRED)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
endif()

set(link_lib glog::glog GTest::gtest)
set(link_math_lib  ${ARMADILLO_LIBRARIES} ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})
aux_source_directory(./test TEST_DIR_FILES)
aux_source_directory(./src SRC_DIR_FILES)

add_executable(block_scholes_part1 main.cpp ${TEST_DIR_FILES} ${SRC_DIR_FILES})
target_link_libraries(block_scholes_part1 ${link_lib} ${link_math_lib} OpenMP::OpenMP_CXX)

target_include_directories(block_scholes_part1 PUBLIC ${glog_INCLUDE_DIR})
target_include_directories(block_scholes_part1 PUBLIC ${GTest_INCLUDE_DIR})
target_include_directories(block_scholes_part1 PUBLIC ${Armadillo_INCLUDE_DIR})
target_include_directories(block_scholes_part1 PUBLIC ./include)
enable_testing()

add_test(NAME run_block_scholes_part1 COMMAND block_scholes_part1)
