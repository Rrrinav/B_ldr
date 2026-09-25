// capture.cpp — grabbing a process's merged output as a string.
//
// There is no separate capture call: run the command with io_out_err{&s}
// and check the exit status yourself. Non-zero exit is not an Err — the
// program ran fine, it just failed — so the pattern is: run, check
// status_code(), read the string.
//
// Optional stdin routing:
//   io_in{&text}    feed text to the child's stdin (borrowed, copied at spawn)
//   io_in{...}      stdin from a borrowed fd, a path, or an eager io_in::open
//   label{...}      label for logging
// Captured text is normalized (\r\n -> \n), a no-op on Linux.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

// Merged stdout+stderr into `out`; true iff the command exited 0.
static auto merged(bld::Cmd cmd, std::string &out) -> bool
{
    auto proc = bld::run(std::move(cmd), bld::io_out_err{&out});
    if (!proc) {
        bld::log::e("spawn failed: {}", proc.error());
        return false;
    }
    return proc->status_code() == 0;
}

int main()
{
    // Merged stdout and stderr.
    std::string out;
    if (!merged(bld::Cmd{"sh", "-c", "echo to-stdout && echo to-stderr >&2"}, out)) {
        return EXIT_FAILURE;
    }
    bld::log::i("merged: '{}'", out); // contains "to-stdout\n" and "to-stderr\n"

    // Merged stream: order is not guaranteed, content is.
    std::string ab;
    if (!merged(bld::Cmd{"sh", "-c", "echo a && echo b >&2"}, ab)) {
        return EXIT_FAILURE;
    }
    bld::log::i("merged: '{}'", ab);

    // Pipe data INTO the child.
    std::string hello = "hello world";
    std::string upper;
    auto proc = bld::run(bld::Cmd{"tr", "a-z", "A-Z"}, bld::io_in{&hello}, bld::io_out{&upper});
    if (!proc || proc->status_code() != 0) {
        bld::log::e("run failed");
        return EXIT_FAILURE;
    }
    bld::log::i("uppercased: '{}'", upper); // "HELLO WORLD"

    // CRLF is normalized by default (meaningful on Windows).
    std::string win;
    if (!merged(bld::Cmd{"sh", "-c", "printf 'line1\\r\\nline2\\r\\n'"}, win)) {
        return EXIT_FAILURE;
    }
    bld::log::i("normalized: '{}'", win); // "line1\nline2\n"

    // A failing command keeps its output: check the code, read the string.
    std::string fail_out;
    auto fail = bld::run(bld::Cmd{"sh", "-c", "echo oops >&2; exit 3"}, bld::io_out_err{&fail_out});
    if (!fail || fail->status_code() == 0) {
        bld::log::e("expected a failing command");
        return EXIT_FAILURE;
    }
    bld::log::i("failed with code {}, output kept: '{}'", fail->status_code(), bld::str::trim(fail_out));

    return EXIT_SUCCESS;
}
