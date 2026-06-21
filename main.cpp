#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"

auto main(int argc, char *argv[]) -> int
{
    bld::rebuild_this_when_needed_ext(argc, argv);

    auto &cfg = bld::Config::get();

    cfg.add_option("--debug", bld::Config::Bool, "Enable verbose output", false)
        .add_option("jobs", bld::Config::Int, "Concurrent jobs", 8)
        .add_option("build-type", bld::Config::String, "Build profile", std::string{"rel"}, {"rel", "debug", "profile"})
        .parse(argc, argv);

    if (cfg["--debug"]) {
        bld::log::d("Verbose mode is enabled!");
    }

    if (cfg["build-type"]) {
        std::string mode = cfg["build-type"];
        bld::log::i("Compiling with mode: {}", mode);
    }

    if (cfg["custom_id"]) {
        std::string id = cfg["custom_id"];
        bld::log::i("Found custom ID: {}", id);
    }
    return 0;
}
