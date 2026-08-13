// config.cpp — command-line option parsing.
//
// Declare options, then read them through cfg["name"] with an implicit conversion.
// parse() NEVER exits: it returns an error for malformed input, and tells you
// when --help was printed so YOU decide the exit path.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main(int argc, char *argv[])
{
    auto &cfg = bld::Config::get();

    cfg.add_option("jobs", bld::Config::Int, "number of parallel jobs", 4)
        .add_option("mode", bld::Config::String, "build mode", std::string{"debug"}, {"debug", "release"})
        .add_option("verbose", bld::Config::Bool, "verbose output", false)
        .add_option("src", bld::Config::String_arr, "extra source files", std::vector<std::string>{});

    // `./config --help` prints a generated usage text (via print_help) and returns help_requested.
    auto res = cfg.parse(argc, argv);
    if (!res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }
    if (res->help_requested) {
        return EXIT_SUCCESS;
    }

    // The Proxy converts to whatever type the option holds:
    //   `./config jobs=8 verbose mode=release src=a.cpp src=b.cpp`
    const int    jobs    = int(cfg["jobs"]);
    const bool   verbose = bool(cfg["verbose"]);
    const auto   mode    = std::string(cfg["mode"]);
    const auto   srcs    = std::vector<std::string>(cfg["src"]);

    bld::log::i("jobs={} verbose={} mode={} extra_srcs={}", jobs, verbose, mode, srcs.size());
    for (const auto &s : srcs) {
        bld::log::i("  src: {}", s);
    }

    // Typed access that reports errors instead of throwing (the Proxy throws).
    if (auto typed = cfg.get_val<int>("jobs")) {
        bld::log::i("typed access: {}", *typed);
    }

    return EXIT_SUCCESS;
}
