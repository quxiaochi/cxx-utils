cmake_minimum_required(VERSION 3.20)

project(cxx-utils C CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-g -Wall -Werror -O1)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(test test)
