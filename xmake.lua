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

target("test-framework")
    set_kind("static")
    add_files("tests/framework/test_helpers.c")
    add_includedirs("tests", {public = true})

target("test-disk")
    set_kind("binary")
    set_default(false)
    add_files("tests/storage/test_disk.c")
    add_deps("test-framework")
    add_files("src/**.c")

target("test-seek")
    set_kind("binary")
    set_default(false)
    add_files("tests/storage/test_seek.c")
    add_deps("test-framework")

target("test-bitmap")
    set_kind("binary")
    set_default(false)
    add_files("tests/space/test_bitmap.c")
    add_deps("test-framework")
    add_files("src/**.c")

target("test-index")
    set_kind("binary")
    set_default(false)
    add_files("tests/metadata/test_index.c")
    add_deps("test-framework")
    add_files("src/**.c")

target("test-alloc")
    set_kind("binary")
    set_default(false)
    add_files("tests/alloc/test_alloc.c")
    add_deps("test-framework")
    add_files("src/**.c")

target("test-dir")
    set_kind("binary")
    set_default(false)
    add_files("tests/logical/test_dir.c")
    add_deps("test-framework")
    add_files("src/**.c")

target("test-vfs")
    set_kind("binary")
    set_default(false)
    add_files("tests/vfs/test_vfs.c")
    add_deps("test-framework")
    add_files("src/**.c")

target("test-integration")
    set_kind("binary")
    set_default(false)
    add_files("tests/integration/test_integration.c")
    add_deps("test-framework")
    add_files("src/**.c")

task("test")
    on_run(function()
        import("core.base.task")
        import("core.base.global")

        local test_targets = {
            "test-disk",
            "test-seek",
            "test-bitmap",
            "test-index",
            "test-alloc",
            "test-dir",
            "test-vfs",
            "test-integration"
        }

        local failed_tests = {}
        local passed_count = 0

        print("Running all test suites\n")

        for _, test_name in ipairs(test_targets) do
            print(string.format("[%s] Starting...", test_name))
            local exit_code = os.execv("xmake", {"run", test_name})
            
            if exit_code == 0 then
                passed_count = passed_count + 1
                print(string.format("[%s] PASSED\n", test_name))
            else
                table.insert(failed_tests, test_name)
                print(string.format("[%s] FAILED (exit code: %d)\n", test_name, exit_code))
            end
        end

        print("\nTest Summary:")
        print(string.format("Total:  %d", #test_targets))
        print(string.format("Passed: %d", passed_count))
        print(string.format("Failed: %d", #failed_tests))
        
        if #failed_tests > 0 then
            print("\nFailed test suites:")
            for _, name in ipairs(failed_tests) do
                print("  - " .. name)
            end
            os.exit(1)
        else
            print("\nAll tests passed!")
        end
    end)

    set_menu {
        usage = "xmake test",
        description = "Run all test suites sequentially"
    }