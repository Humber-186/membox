cmake_minimum_required(VERSION 3.15)
project(membox LANGUAGES CXX)

# set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# find external dependencies from system libraries
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

# Static library SV
add_library(SV STATIC
  src/sv_basic.cpp
  src/sv_supervisor.cpp
  src/buddy.cpp
)
target_include_directories(SV PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(SV PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(SV PUBLIC spdlog::spdlog fmt::fmt)
set_target_properties(SV PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Executable membox-test
add_executable(membox-test src/main.cpp)
target_compile_options(membox-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(membox-test PRIVATE SV spdlog::spdlog fmt::fmt)
