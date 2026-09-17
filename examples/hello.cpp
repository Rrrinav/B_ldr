// hello.cpp — the smallest possible build script.
//
// A bld script is just a C++ program. This one:
//   1. recompiles itself whenever hello.cpp (or b_ldr.hpp) changes,
//   2. logs a greeting,
//   3. runs a shell-free command and checks its exit code.
//
// Build & run from the repo root:
//   g++ -std=c++23 examples/hello.cpp -o hello
//   ./hello

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main(int argc, char *argv[])
{
    // If hello.cpp is newer than ./hello, recompile and re-run automatically.
    if (auto res = bld::rebuild_this_when_needed_ext(argc, argv); !res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }

    bld::log::i("Hello from a build script!");

    // bld::Cmd is a command split into arguments — no shell, no quoting pitfalls.
    auto proc = bld::run(bld::Cmd{"g++", "--version"});
    if (proc && proc->status_code() == 0) {
        bld::log::i("Your compiler works.");
    } else {
        bld::log::e("Compiler invocation failed.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
