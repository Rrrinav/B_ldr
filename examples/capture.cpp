// capture.cpp — grabbing a process's merged output as a string.
//
// bld::capture always captures merged stdout+stderr and returns it:
//   bld::capture(cmd) -> expected<string, Err>  (merged output, exit 0 only)
// Optional stdin modifiers:
//   io_in{&text}    feed text to the child's stdin (borrowed, copied at spawn)
//   io_in{...}      stdin from a borrowed fd, a path, or an eager io_in::open
//   label{...}      label for logging
//   raw_crlf{}      keep "\r\n" as-is (default normalizes to "\n").
// Non-zero exit becomes an unexpected Err with the merged output
// in Err::output.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    // Merged stdout and stderr.
    if (auto out = bld::capture(bld::Cmd{"sh", "-c", "echo to-stdout && echo to-stderr >&2"}); !out) {
        bld::log::e("capture failed: {}", out.error());
        return EXIT_FAILURE;
    } else {
        bld::log::i("merged: '{}'", *out); // contains "to-stdout\n" and "to-stderr\n"
    }

    // Merged stream: order is not guaranteed, content is.
    if (auto merged = bld::capture(bld::Cmd{"sh", "-c", "echo a && echo b >&2"}); !merged) {
        bld::log::e("capture failed: {}", merged.error());
        return EXIT_FAILURE;
    } else {
        bld::log::i("merged: '{}'", *merged);
    }

    // Pipe data INTO the child.
    std::string hello = "hello world";
    if (auto result = bld::capture(bld::Cmd{"tr", "a-z", "A-Z"}, bld::io_in{&hello}); !result) {
        bld::log::e("capture failed: {}", result.error());
        return EXIT_FAILURE;
    } else {
        bld::log::i("uppercased: '{}'", *result); // "HELLO WORLD"
    }

    // CRLF is normalized by default (meaningful on Windows).
    if (auto win = bld::capture(bld::Cmd{"sh", "-c", "printf 'line1\\r\\nline2\\r\\n'"}); !win) {
        bld::log::e("capture failed: {}", win.error());
        return EXIT_FAILURE;
    } else {
        bld::log::i("normalized: '{}'", *win); // "line1\nline2\n"
    }

    // Opt out of the normalization when you need the bytes as-is.
    if (auto raw = bld::capture(bld::Cmd{"sh", "-c", "printf 'a\\r\\nb\\r\\n'"}, bld::raw_crlf{}); !raw) {
        bld::log::e("capture failed: {}", raw.error());
        return EXIT_FAILURE;
    } else {
        bld::log::i("raw: '{}'", *raw); // "a\r\nb\r\n"
    }

    return EXIT_SUCCESS;
}
