// files.cpp — the filesystem helpers.
//
// Everything returns std::expected — an Err carries a message you can format into logs.
// Dir_walker is a small fluent API for walking trees with filters.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    // Write / read / append. Binary-safe.
    if (auto res = bld::fs::write_file("demo.txt", "hello"); !res) {
        bld::log::e("write failed: {}", res.error());
        return EXIT_FAILURE;
    }
    if (auto res = bld::fs::append_file("demo.txt", " world"); !res) {
        bld::log::e("append failed: {}", res.error());
        return EXIT_FAILURE;
    }
    auto content = bld::fs::read_file("demo.txt");
    if (content) {
        bld::log::i("read back: '{}'", *content); // "hello world"
    }

    // Path helpers.
    bld::log::i("joined: {}", bld::fs::join("build", "obj", "app.o")); // build/obj/app.o
    bld::log::i("stem of 'src/main.cpp' is '{}'", bld::fs::stem("src/main.cpp"));

    // Directories. make_dirs creates parents too; remove removes anything (files or trees).
    if (auto res = bld::fs::make_dirs("demo/sub"); !res) {
        bld::log::e("mkdir failed: {}", res.error());
        return EXIT_FAILURE;
    }
    if (auto res = bld::fs::write_file("demo/sub/note.txt", "hi"); !res) {
        bld::log::e("write failed: {}", res.error());
        return EXIT_FAILURE;
    }
    bld::log::i("demo/sub/note.txt exists: {}", bld::fs::exists("demo/sub/note.txt"));

    // Walk a tree with filters, skip dirs by name, collect paths.
    auto cpp_files = bld::fs::Dir_walker{"."}
        .ext(".cpp")
        .skip({".git", "build"})
        .collect_paths();
    if (cpp_files) {
        bld::log::i("found {} .cpp files:", cpp_files->size());
        for (const auto &p : *cpp_files) {
            bld::log::i("  {}", p.string());
        }
    }

    // High-level one-liners.
    auto headers = bld::fs::find_by_ext(".", ".hpp");
    bld::log::i("{} .hpp files found", headers ? headers->size() : 0);

    // Everything is expected-based: check errors instead of catching exceptions.
    if (auto res = bld::fs::read_file("does_not_exist.txt"); !res) {
        bld::log::w("expected failure: {}", res.error());
    }

    // Cleanup.
    std::ignore = bld::fs::remove("demo", "demo.txt");
    return EXIT_SUCCESS;
}
