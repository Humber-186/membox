add_rules("mode.debug", "mode.release")

target("membox")
    set_kind("binary")
    add_languages("c++20")
    add_files("src/*.cpp")
    add_links("fmt")
