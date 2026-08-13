// logging.cpp — the logging API.
//
// Five severity levels: DEBUG < INFO < WARN < ERROR < FATAL.
// By default only INFO and above are printed; colors are on when stderr is a terminal.
// The logger can be redirected to any std::ostream, or replaced entirely.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    bld::log::d("You will not see this by default");
    bld::log::i("info level");
    bld::log::w("warning level");
    bld::log::e("error level");

    // printf-style formatting, but type-safe at compile time.
    bld::log::i("Formatted: {} + {} = {}", 2, 3, 2 + 3);

    // Show everything, including DEBUG lines.
    bld::log::set_min_level(bld::Logger::Level::dbg);
    bld::log::d("Now you see me");

    // Direct a message at a specific stream (useful for log files).
    bld::log::i(std::cout, "This one goes to stdout instead of stderr");

    // Replace the default formatter with your own.
    bld::Logger::set_logger_fn([](std::ostream &os, const bld::Logger::Log_record &r) {
        std::println(os, "[custom] level={} msg={}", static_cast<int>(r.lvl), r.str);
    });
    bld::log::i("Rendered by my custom logger");
    return EXIT_SUCCESS;
}
