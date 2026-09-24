// walk.cpp — callback-driven directory traversal.
//
// g++ -std=c++23 -I. examples/walk.cpp -o /tmp/walk && /tmp/walk
//
// One function walks everything: walk(root, opts, visitor). The visitor sees
// every directory (so it can prune) and every non-hidden file, and answers
// per entry: next (continue), prune (skip this branch), stop (done, success),
// fail (abort the whole walk with a user error).

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

namespace fs = std::filesystem;

static void make_demo_tree(const std::string &root)
{
    fs::remove_all(root);
    fs::create_directories(root + "/src/gen/deep");
    fs::create_directories(root + "/build");
    auto touch = [](const std::string &p) {
        if (FILE *f = std::fopen(p.c_str(), "w")) {
            std::fputs("x", f);
            std::fclose(f);
        }
    };
    touch(root + "/src/main.cpp");
    touch(root + "/src/util.hpp");
    touch(root + "/src/gen/inner.cpp");
    touch(root + "/src/gen/deep/bottom.cpp"); // stays unvisited when pruned
    touch(root + "/build/out.o");
}

int main()
{
    const std::string root = "demo_walk";
    make_demo_tree(root);
    const std::string src = root + "/src";

    // 1. Collect: the visitor keeps what it wants, ignores the rest.
    std::vector<std::string> cpps;
    if (auto r = bld::fs::walk(src, {.skip = {"gen"}}, [&](const auto &e) {
            if (e.is_file() && e.extension() == ".cpp") {
                cpps.push_back(e.filename());
            }
            return bld::fs::Act::next;
        });
        !r) {
        bld::log::e("walk failed: {}", r.error().message());
    }
    bld::log::i("cpps (gen pruned): {}", cpps.size()); // 1: main.cpp

    // 2. Dynamic prune: decide per directory, inside the callback.
    std::vector<std::string> kept;
    std::ignore = bld::fs::walk(
        src, {}, [&](const auto &e) {
            if (e.is_dir() && e.filename() == "gen") {
                return bld::fs::Act::prune; // neither visited nor descended
            }
            if (e.is_file()) {
                kept.push_back(e.filename());
            }
            return bld::fs::Act::next;
        });
    bld::log::i("kept (gen pruned): {}", kept.size()); // 2: main.cpp, util.hpp

    // 3. stop: first match ends the walk cleanly (success, not an error).
    std::string first_hpp;
    if (auto r = bld::fs::walk(src, {}, [&](const auto &e) {
            if (e.is_file() && e.extension() == ".hpp") {
                first_hpp = e.filename();
                return bld::fs::Act::stop;
            }
            return bld::fs::Act::next;
        });
        !r) {
        bld::log::e("walk failed: {}", r.error().message());
    }
    bld::log::i("first hpp: {}", first_hpp); // util.hpp

    // 4. fail: abort everything with a USER error (distinct from fs errors).
    auto bad = bld::fs::walk(src, {}, [](const auto &) { return bld::fs::Act::fail; });
    if (!bad) {
        bld::log::i("aborted: user={} msg='{}'", bad.error().is_user_error(), bad.error().message());
    }

    // 5. Library failure looks different: missing root is an fs error.
    auto missing = bld::fs::walk(root + "/nope", {}, [](const auto &) { return bld::fs::Act::next; });
    if (!missing) {
        bld::log::i("missing root: fs={} msg='{}'", missing.error().is_fs_error(), missing.error().message());
    }

    // 6. Eager one-liners for the common case.
    bld::log::i("files(): {}", bld::fs::files(src).size());
    auto by_ext = bld::fs::find_by_ext(src, "cpp", ".hpp");
    bld::log::i("find_by_ext: {}", by_ext ? by_ext->size() : 0);

    fs::remove_all(root);
    return EXIT_SUCCESS;
}
