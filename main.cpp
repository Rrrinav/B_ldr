#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"

auto &cfg = bld::Config::get();

using namespace std::string_view_literals;

const std::string TEST_DIR{"./tests/"};

auto run_tests() -> bool
{
    bld::log::i("Building test executable");
    bld::Cmd cmd{"g++", "-o", "test", TEST_DIR + "main.cpp", "-std=c++23", "-I."};
#ifdef _WIN32
#ifdef __GNUC__
    cmd.push("-lstdc++exp");
#endif
#endif
    if (auto res = bld::run(cmd); !res) {
        return false;
    } else {
#ifdef _WIN32
        if (auto res_run = bld::run(bld::Cmd{"./test.exe"}); !res_run) {
            bld::log::e("Test script run failed.");
            return false;
        }
#else
        if (auto res_run = bld::run(bld::Cmd{"./test"}); !res_run) {
            bld::log::e("Test script run failed.");
            return false;
        }
#endif
    }

    bld::log::i("Test script ran successfully.");
    return true;
}

int main(int argc, char *argv[])
{
    std::ignore = bld::rebuild_this_when_needed_ext(argc, argv);
    std::ignore = cfg.parse(argc, argv);

    bld::time::stamp t1{};

    if (cfg["test"] || cfg["-test"]) {
        if (run_tests()) {
            bld::log::i("Tests executed in {}", bld::time::format(t1.elapsed()));
            return EXIT_SUCCESS;
        } else {
            return EXIT_FAILURE;
        }
    }

    bld::time::stamp t2{};

    auto d1 = bld::time::since(t1);
    bld::log::i("Bootstrap sequence took: {}", bld::time::format(d1));
    return 0;
}