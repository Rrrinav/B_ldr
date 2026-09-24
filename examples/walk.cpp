// walk.cpp — controller-driven directory traversal.
//
// g++ -std=c++23 -I. examples/walk.cpp -o /tmp/walk && /tmp/walk
//
// walk_dir() returns a lazy range bound to your controller. The loop body
// steers it with plain control flow: dont_recurse skips a branch, break ends
// cleanly, abort() quits with a user error. Ranges compose downstream.

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

    // 1. The canonical shape: static skip + pipes + plain control flow.
    // NOTE: a directory hidden by a downstream filter never reaches the body,
    // so it cannot be dynamically pruned — prune statically via opts.skip,
    // or filter in the body (see 1b).
    {
        bld::fs::Controller ctl;
        ctl.opts.skip = {"build", "gen"};
        std::vector<std::string> cpps;
        for (const auto &entry :
             bld::fs::walk_dir(src, ctl) | bld::fs::only_extensions("cpp", "hpp", "cppm") | bld::fs::not_name("main.cpp")) {
            if (entry.is_file()) {
                cpps.push_back(entry.filename());
            }
        }
        if (ctl.failed()) {
            bld::log::e("walk failed: {}", ctl.error().message());
        }
        bld::log::i("cpps (gen skipped, main.cpp excluded): {}", cpps.size()); // 1: util.hpp
    }

    // 1b. Dynamic prune: no ext filter upstream, so gen reaches the body.
    {
        bld::fs::Controller ctl;
        std::vector<std::string> kept;
        for (const auto &entry : bld::fs::walk_dir(src, ctl)) {
            if (entry.is_dir() && entry.filename() == "gen") {
                ctl.dont_recurse = true;
                continue;
            }
            if (entry.is_file() && entry.extension() == ".cpp") {
                kept.push_back(entry.filename());
            }
        }
        bld::log::i("kept (gen pruned in-body): {}", kept.size()); // 1: main.cpp
    }

    // 2. break ends cleanly: no error recorded.
    {
        std::string first_hpp;
        bld::fs::Controller ctl;
        for (const auto &e : bld::fs::walk_dir(src, ctl)) {
            if (e.is_file() && e.extension() == ".hpp") {
                first_hpp = e.filename();
                break;
            }
        }
        bld::log::i("first hpp: {} (failed={})", first_hpp, ctl.failed()); // util.hpp, false
    }

    // 3. abort() quits with a USER error (distinct from fs errors).
    {
        bld::fs::Controller ctl;
        for (const auto &e : bld::fs::walk_dir(src, ctl)) {
            (void)e;
            ctl.abort("nope");
            break;
        }
        bld::log::i("aborted: user={} msg='{}'", ctl.failed() && ctl.error().is_user_error(), ctl.error().message());
    }

    // 4. Library failure looks different: missing root is an fs error.
    {
        bld::fs::Controller ctl;
        for (const auto &e : bld::fs::walk_dir(root + "/nope", ctl)) {
            (void)e;
        }
        bld::log::i("missing root: fs={} msg='{}'", ctl.failed() && ctl.error().is_fs_error(), ctl.error().message());
    }

    // 5. Eager one-liners for the common case.
    bld::log::i("files(): {}", bld::fs::files(src).size());
    auto by_ext = bld::fs::find_by_ext(src, "cpp", ".hpp");
    bld::log::i("find_by_ext: {}", by_ext ? by_ext->size() : 0);

    fs::remove_all(root);
    return EXIT_SUCCESS;
}
