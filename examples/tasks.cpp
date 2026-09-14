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
    auto res = bld::run(tasks, bld::use_threads{4});
    if (res) {
        bld::log::i("4 parallel sleeps took {}", bld::time::format(t0.elapsed()));
    }

    // A failing task poisons the batch: remaining tasks are never scheduled.
    std::vector<bld::Task> fragile;
    fragile.emplace_back(bld::Cmd{"true"});
    fragile.emplace_back(bld::Cmd{"false"});
    fragile.emplace_back(bld::Cmd{"true"});

    auto bad = bld::run(fragile, bld::use_threads{2});
    if (!bad) {
        const auto &report = std::any_cast<const bld::Run_result &>(bad.error().payload);
        bld::log::e("run failed after {} task(s): {}", report.ran, bad.error());
    }

    // use_threads{nullopt} (default) => max-1. Positive => capped by max.
    // Add inputs/outputs + deduce_dependency to make this same call graph-aware.
    bld::log::i("default parallelism would be {} threads", bld::max_thread_count());

    return EXIT_SUCCESS;
}
