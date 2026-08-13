// capture.cpp — grabbing a process's output into strings.
//
// bld::capture is like bld::run but it captures stdout/stderr into std::string buffers:
//   cap_out{str}    capture stdout
//   cap_err{str}    capture stderr
//   cap_merge{str}  capture both, merged into one stream
//   in_str{text}    feed text to the child's stdin
// Captured text is always normalized: "\r\n" becomes "\n" (needed on Windows, where
// every program emits CRLF; on Linux it's a no-op). Pass bld::raw_crlf{} to disable.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    // Separate stdout and stderr.
    std::string out, err;
    auto status = bld::capture(bld::Cmd{"sh", "-c", "echo to-stdout && echo to-stderr >&2"},
                               bld::cap_out{out}, bld::cap_err{err});
    if (status && status->code == 0) {
        bld::log::i("stdout: '{}'", out); // "to-stdout\n"
        bld::log::i("stderr: '{}'", err); // "to-stderr\n"
    }

    // Merged stream: order is not guaranteed, content is.
    std::string merged;
    if (auto st = bld::capture(bld::Cmd{"sh", "-c", "echo a && echo b >&2"}, bld::cap_merge{merged}); !st) {
        bld::log::e("capture failed: {}", st.error());
        return EXIT_FAILURE;
    }
    bld::log::i("merged: '{}'", merged);

    // Pipe data INTO the child.
    std::string result;
    if (auto st = bld::capture(bld::Cmd{"tr", "a-z", "A-Z"}, bld::cap_out{result}, bld::in_str{"hello world"}); !st) {
        bld::log::e("capture failed: {}", st.error());
        return EXIT_FAILURE;
    }
    bld::log::i("uppercased: '{}'", result); // "HELLO WORLD\n"

    // CRLF is normalized by default (meaningful on Windows).
    std::string win;
    if (auto st = bld::capture(bld::Cmd{"sh", "-c", "printf 'line1\\r\\nline2\\r\\n'"}, bld::cap_out{win});
        !st) {
        bld::log::e("capture failed: {}", st.error());
        return EXIT_FAILURE;
    }
    bld::log::i("normalized: '{}'", win); // "line1\nline2\n"

    // Opt out of the normalization when you need the bytes as-is.
    std::string raw;
    if (auto st = bld::capture(bld::Cmd{"sh", "-c", "printf 'a\\r\\nb\\r\\n'"}, bld::cap_out{raw}, bld::raw_crlf{});
        !st) {
        bld::log::e("capture failed: {}", st.error());
        return EXIT_FAILURE;
    }
    bld::log::i("raw: '{}'", raw); // "a\r\nb\r\n"

    return EXIT_SUCCESS;
}
