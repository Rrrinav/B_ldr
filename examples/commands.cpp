// commands.cpp — running processes.
//
// bld::run always returns std::expected<bld::Proc, bld::Err>.
// By default it waits for the process and you get a Proc holding the final status.
// With bld::async{} it returns immediately and you call wait() yourself.
// Output streams can be redirected to files with out_f / err_f / in_f.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    // Sync run: blocks until the process exits.
    auto proc = bld::run(bld::Cmd{"true"});
    if (!proc) {
        bld::log::e("failed to spawn: {}", proc.error());
        return EXIT_FAILURE;
    }
    bld::log::i("true exited with code {}", proc->status_code());

    // A non-zero exit is not an Err — the program ran fine, it just failed.
    auto failing = bld::run(bld::Cmd{"false"});
    bld::log::i("false exited with code {}", failing->status_code());

    // Async: the process keeps running; you decide when to wait for it.
    auto sleeper = bld::run(bld::Cmd{"sleep", "1"}, bld::async{});
    bld::log::i("sleeping process is running: {}", sleeper->is_running());
    auto status = sleeper->wait();
    if (status) {
        bld::log::i("sleep finished: {}", *status);
    }

    // Redirect stdout and stderr to files. Paths must already have their parent dirs.
    auto redirected = bld::run(bld::Cmd{"g++", "--version"}, bld::out_f{"gcc_version.txt"});
    if (redirected) {
        bld::log::i("wrote gcc_version.txt");
        std::filesystem::remove("gcc_version.txt");
    }

    // Kill a runaway process.
    auto runaway = bld::run(bld::Cmd{"sleep", "60"}, bld::async{});
    runaway->kill();
    if (auto st = runaway->wait(); !st) {
        bld::log::e("wait after kill failed: {}", st.error());
        return EXIT_FAILURE;
    }
    bld::log::i("killed the runaway");

    return EXIT_SUCCESS;
}
