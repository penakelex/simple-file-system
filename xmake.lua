add_rules("mode.debug", "mode.release")

set_languages("clatest")
set_warnings("all", "extra")
add_includedirs("include", {public = true})

if is_plat("windows") then
    add_defines("_CRT_SECURE_NO_WARNINGS")
else
    add_defines("_POSIX_C_SOURCE=200809L", "_FILE_OFFSET_BITS=64")
end

target("simple-file-system")
    set_kind("binary")
    add_files("src/**.c")
    add_files("cli/*.c")

target("fs-seek-test")
    set_kind("binary")
    add_files("tests/seek_test.c")