add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_requires("spdlog")

-- set_policy("build.sanitizer.address", true)
-- set_policy("build.sanitizer.leak", true)
-- set_policy("build.sanitizer.undefined", true)

target("SV")
    set_kind("static")
    add_languages("c++20")
    add_files("src/sv_basic.cpp", "src/sv_supervisor.cpp", "src/buddy.cpp")
    add_links("spdlog", "fmt")
    add_cxxflags("-fPIC", "-Wall")

target("membox-test")
    set_kind("binary")
    add_languages("c++20")
    add_deps("SV")
    add_files("src/main.cpp")
    add_links("spdlog", "fmt")
    add_cxxflags("-Wall")
    set_default("false")

