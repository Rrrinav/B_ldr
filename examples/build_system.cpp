// build_system.cpp — a small real incremental build.
//
// This script builds a tiny two-file program:
//   demo_src/main.cpp  -> demo_build/main.o
//   demo_src/util.cpp  -> demo_build/util.o
//   link both objects  -> demo_build/app
//
// It demonstrates the unified runner:
//   1. declare inputs and outputs,
//   2. run independent objects in parallel,
//   3. let the runner unlock the linker once they finish.
//
// Run it twice — the second run does nothing, because everything is up to date.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main(int argc, char *argv[])
{
    // Rebuild this script itself when build_system.cpp or b_ldr.hpp changes.
    if (auto res = bld::rebuild_this_when_needed_ext(argc, argv); !res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }

    // Create the toy project if it doesn't exist yet.
    if (auto res = bld::fs::make_dirs("demo_src", "demo_build"); !res) {
        bld::log::e("mkdir failed: {}", res.error());
        return EXIT_FAILURE;
    }
    if (!bld::fs::exists("demo_src/main.cpp")) {
        if (auto w1 = bld::fs::write_file("demo_src/main.cpp", "#include \"util.hpp\"\nint main() { return util() + 1; }\n"); !w1) {
            bld::log::e("write failed: {}", w1.error());
            return EXIT_FAILURE;
        }
        if (auto w2 = bld::fs::write_file("demo_src/util.hpp", "#pragma once\nint util();\n"); !w2) {
            bld::log::e("write failed: {}", w2.error());
            return EXIT_FAILURE;
        }
        if (auto w3 = bld::fs::write_file("demo_src/util.cpp", "#include \"util.hpp\"\nint util() { return 41; }\n"); !w3) {
            bld::log::e("write failed: {}", w3.error());
            return EXIT_FAILURE;
        }
    }

    bld::Plan build;
    build.add("main.o", bld::Cmd{"g++", "-c", "demo_src/main.cpp", "-o", "demo_build/main.o"});
    build.needs_from("main.o", {"demo_src/main.cpp", "demo_src/util.hpp"});
    build.produces("main.o", "demo_build/main.o");
    build.mark_compile_command("main.o");
    build.add("util.o", bld::Cmd{"g++", "-c", "demo_src/util.cpp", "-o", "demo_build/util.o"});
    build.needs_from("util.o", {"demo_src/util.cpp", "demo_src/util.hpp"});
    build.produces("util.o", "demo_build/util.o");
    build.mark_compile_command("util.o");
    build.add("app", bld::Cmd{"g++", "demo_build/main.o", "demo_build/util.o", "-o", "demo_build/app"});
    build.needs_from("app", {"demo_build/main.o", "demo_build/util.o"});
    build.produces("app", "demo_build/app");

    if (auto res = bld::run(build, bld::jobs{4}, bld::write_compile_commands{"demo_build/compile_commands.json"}); !res) {
        bld::log::e("build failed: {}", res.error());
        return EXIT_FAILURE;
    }

    bld::log::i("build complete: demo_build/app");
    return EXIT_SUCCESS;
}
