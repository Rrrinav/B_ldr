// tasks.cpp — running commands in parallel.
//
// bld::Task pairs a command with its config. Collect them, then bld::run(tasks, max_jobs)
// executes up to max_jobs at a time (batch style), or bld::run_threaded(tasks, threads)
// schedules them on a worker-thread pool. Both return std::expected<void, Err>.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    std::vector<bld::Task> tasks;
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});
    tasks.emplace_back(bld::Cmd{"sleep", "0.5"});

    // All four run concurrently; the whole batch finishes in ~0.5s, not 2s.
    auto t0 = bld::time::now();
    auto res = bld::run(tasks, 4);
    if (res) {
        bld::log::i("4 parallel sleeps took {}", bld::time::format(t0.elapsed()));
    }

    // A failing task poisons the batch: remaining tasks are never scheduled.
    std::vector<bld::Task> fragile;
    fragile.emplace_back(bld::Cmd{"true"});
    fragile.emplace_back(bld::Cmd{"false"});
    fragile.emplace_back(bld::Cmd{"true"});

    auto bad = bld::run(fragile, 2);
    if (!bad) {
        const auto completed = std::any_cast<std::size_t>(bad.error().payload);
        bld::log::e("batch failed after {} completed tasks: {}", completed, bad.error());
    }

    // Same idea, but with a real worker-thread pool: run_threaded(tasks, threads).
    // The first failure cancels the remaining queued tasks; tasks already running
    // are allowed to finish. threads == 0 means "use hardware_concurrency".
    auto t1 = bld::time::now();
    auto threaded = bld::run_threaded(tasks, 4);
    if (threaded) {
        bld::log::i("4 threaded sleeps took {}", bld::time::format(t1.elapsed()));
    }

    auto bad_threaded = bld::run_threaded(fragile, 2);
    if (!bad_threaded) {
        bld::log::e("threaded pool failed: {}", bad_threaded.error());
    }

    // max_jobs == 0 means "use hardware_concurrency".
    bld::log::i("default parallelism would be {} jobs", std::thread::hardware_concurrency());

    return EXIT_SUCCESS;
}
