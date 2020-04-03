cmake_minimum_required (VERSION 3.8)
project ("coyote" CXX)

set(CMAKE_CXX_STANDARD 11)
enable_testing()

add_subdirectory(src/scheduler)
