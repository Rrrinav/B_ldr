// build_system.cpp — a small real incremental build.
//
// This script builds a tiny two-file program:
//   demo_src/main.cpp  -> demo_build/main.o
//   demo_src/util.cpp  -> demo_build/util.o
//   link both objects  -> demo_build/app
//
// It demonstrates the core loop of every build system:
//   1. only rebuild what is_outdated (newer sources, missing output),
//   2. compile independent objects in parallel,
//   3. link only after every object is done.
//
// Run it twice — the second run does nothing, because everything is up to date.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

struct Job
{
    std::string out;
    std::vector<std::string> deps; // outputs that feed into this job
    bld::Cmd cmd;
};

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

    // Phase 1: compile the objects. main.o and util.o don't depend on each other,
    // so they are scheduled together and run in parallel.
    const std::vector<Job> compiles = {
        {"demo_build/main.o",
         {"demo_src/main.cpp", "demo_src/util.hpp"},
         bld::Cmd{"g++", "-c", "demo_src/main.cpp", "-o", "demo_build/main.o"}},
        {"demo_build/util.o",
         {"demo_src/util.cpp", "demo_src/util.hpp"},
         bld::Cmd{"g++", "-c", "demo_src/util.cpp", "-o", "demo_build/util.o"}},
    };

    std::vector<bld::Task> pending;
    for (const auto &job : compiles) {
        if (bld::is_outdated(job.out, job.deps)) {
            bld::log::i("scheduling: {}", job.out);
            pending.emplace_back(job.cmd);
        } else {
            bld::log::i("up to date : {}", job.out);
        }
    }
    if (!pending.empty()) {
        if (auto res = bld::run(pending, 4); !res) {
            bld::log::e("compile phase failed: {}", res.error());
            return EXIT_FAILURE;
        }
    }

    // Phase 2: link. Runs only after the compiles finished (they're joined above).
    const Job link{"demo_build/app",
                   {"demo_build/main.o", "demo_build/util.o"},
                   bld::Cmd{"g++", "demo_build/main.o", "demo_build/util.o", "-o", "demo_build/app"}};
    if (bld::is_outdated(link.out, link.deps)) {
        bld::log::i("linking  : {}", link.out);
        auto linked = bld::run(link.cmd);
        if (!linked || linked->status_code() != 0) {
            bld::log::e("link failed");
            return EXIT_FAILURE;
        }
    } else {
        bld::log::i("up to date : {}", link.out);
    }

    bld::log::i("build complete: demo_build/app");
    return EXIT_SUCCESS;
}
