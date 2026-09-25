// tasks.cpp — one runner for commands, parallel work, and dependency graphs.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    std::vector<bld::Task> tasks;
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});

    // All four run concurrently; the whole run finishes in ~0.5s, not 2s.
    auto t0 = bld::time::now();
    auto res = bld::run(tasks, bld::jobs{4});
    if (res) {
        bld::log::i("4 parallel sleeps took {}", bld::time::format(t0.elapsed()));
    }

    // A failing task poisons the batch: remaining tasks are never scheduled.
    std::vector<bld::Task> fragile;
    fragile.emplace_back(bld::Cmd{"true"});
    fragile.emplace_back(bld::Cmd{"false"});
    fragile.emplace_back(bld::Cmd{"true"});

    auto bad = bld::run(fragile, bld::jobs{2});
    if (!bad) {
        bld::log::e("run failed: {}", bad.error());
    }

    // A group task holds subtasks instead of a command: they run as
    // dotted-name leaves ("build.compile") sharing the group's edges.
    bld::Task build;
    build.name = "build";
    bld::Task compile{bld::Cmd{"sleep", "0.5"}};
    compile.name = "compile";
    bld::Task link{bld::Cmd{"sleep", "0.5"}};
    link.name = "link";
    build.sub(std::move(compile)).sub(std::move(link));

    bld::Plan plan;
    plan.add(std::move(build));
    plan.add("test", bld::Cmd{"true"});
    plan.after("test", "build"); // runs after BOTH leaves
    if (auto grouped = bld::run(plan, bld::jobs{4}); grouped) {
        bld::log::i("grouped plan ran={} skipped={}", grouped->ran, grouped->skipped);
    }

    // jobs{} (default) => max-1. jobs{n} => exactly n, clamped to the machine.
    // A bare span always runs everything; use a Plan for ordered builds.
    bld::log::i("default parallelism would be {} procs", bld::max_parallel_count());

    return EXIT_SUCCESS;
}
