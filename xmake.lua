add_rules("mode.debug", "mode.release")

target("simple-file-system")
    set_kind("binary")
    set_languages("c23")
    add_cflags("-Wall")
    add_includedirs("include", {public = true})

    add_files("src/*.c")
    add_files("cli/*.c")
