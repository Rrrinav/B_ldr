#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"

auto &cfg = bld::Config::get();

using namespace std::string_view_literals;

const std::string TEST_DIR{"./tests/"};

auto main(int argc, char *argv[]) -> int
{
    bld::rebuild_this_when_needed_ext(argc, argv);

    cfg.parse(argc, argv);

    if (cfg["test"]) {
        bld::log::i("test is set");
        if (auto res = bld::run(bld::Cmd{"g++", "-o", "test", TEST_DIR + "main.cpp", "-std=c++23"}); !res) {
            return 1;
        } else {
            if (auto res_run = bld::run(bld::Cmd{"./test"}); !res_run) {
                bld::log::e("Test script run failed.");
            } else {
                bld::log::i("Test script ran successfully.");
            }
        }
    }
    return 0;
}
