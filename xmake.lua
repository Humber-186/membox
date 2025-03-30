add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_requires("spdlog")

target("SV32")
    set_kind("static")
    add_languages("c++20")
    add_files("src/sv32_basic.cpp", "src/sv32_supervisor.cpp", "src/buddy.cpp")
    add_links("spdlog", "fmt")

target("SV39")
    set_kind("static")
    add_languages("c++20")
    add_files("src/sv39_basic.cpp", "src/sv39_supervisor.cpp", "src/buddy.cpp")
    add_links("spdlog", "fmt")

target("membox-test")
    set_kind("binary")
    add_languages("c++20")
    add_deps("SV32", "SV39")
    add_files("src/main.cpp")
    add_links("spdlog", "fmt")

