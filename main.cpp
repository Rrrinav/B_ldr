#include <source_location>
#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"

auto &cfg = bld::Config::get();

using namespace std::string_view_literals;

const std::string TEST_DIR{"./tests/"};

auto run_tests() -> bool
{
    if (auto res = bld::run(bld::Cmd{"g++", "-o", "test", TEST_DIR + "main.cpp", "-std=c++23"}); !res) {
        return false;
    } else {
        if (auto res_run = bld::run(bld::Cmd{"./test"}); !res_run) {
            bld::log::e("Test script run failed.");
            return false;
        }
    }

    bld::log::i("Test script ran successfully.");
    return true;
}

namespace bld {

}; // namespace bld

auto main(int argc, char *argv[]) -> int
{
    bld::rebuild_this_when_needed_ext(argc, argv);
    cfg.parse(argc, argv);

    return 0;

    if (cfg["test"]) {
        if (run_tests()) {
            return EXIT_SUCCESS;
        } else {
            return EXIT_FAILURE;
        }
    }
    return 0;
}
