cmake_minimum_required(VERSION 3.15)
project(membox LANGUAGES CXX)

# set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# options
option(ENABLE_SANITIZER "Enable building with sanitizer support" OFF)

# find external dependencies from system libraries
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

# Common settings
if (ENABLE_SANITIZER)
    message(STATUS "Building with sanitizer support")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak,undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,leak,undefined")
endif()

# Static library SV
add_library(SV STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sv_basic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sv_supervisor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buddy.cpp
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
