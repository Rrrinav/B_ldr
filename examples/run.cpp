// run.cpp — every way to use bld::run, and every way to misconfigure it.
//
// g++ -std=c++23 examples/run.cpp -o /tmp/run && /tmp/run
// clang++ -std=c++23 examples/run.cpp -o /tmp/run && /tmp/run
//
// OVERLOADS (all of bld::run):
//   run(Cmd_loc, Proc_modifiers...) -> expected<Proc, Err>   single process
//   run(span<Task>, Run_modifiers...) -> expected<Run_result, Err>   batch
//   run(Plan&, Run_modifiers...) -> expected<Run_result, Err>        graph build
//   run(Compilation_database, Run_modifiers...) -> expected<Run_result, Err>
//   + wait_all(span<Proc>) -> expected<size_t, Err>  (reaps detached async procs)
//
// PROC MODIFIERS (per command/task — Config_modifier_c, consume Proc_config):
//   async{}                 return running Proc instead of waiting
//   label{"name"}           log label (at most one)
//   cwd{"dir"}              child working dir, must exist (at most one)
//   dry_run{}               log-only preview: log what would run, spawn nothing
//                           (also on capture() and per-Task in batches)
//   out_fd{fd} / err_fd / in_fd          borrowed fds (Fd_view; Owned_Fd converts implicitly)
//   out_err_fd{fd}                       one borrowed fd for out+err (merges)
//   out_file / err_file / in_file        eager files (Shared_fd, lifetime-safe)
//   out_err_file                         one eager file for out+err (merges)
//   out_str{s} / err_str{s} / out_err_str{s}  capture into strings (borrowed,
//                           must outlive wait; only out_err_str merges)
//   lazy_out_file{p} / lazy_err_file / lazy_in_file / lazy_out_err_file
//                           paths, opened lazily at spawn (at most one per stream)
//   pipe{.out,.err,.in,.merge_err_and_out}  monolithic routing, mixes with nothing
//   Rules: <=1 of each in/out/err group, pipe mixes with nothing, no in_str here.
//
// RUN MODIFIERS (whole batch only — Run_modifier_c, consume Run_config):
//   jobs{[opt]int}         nullopt=>max-1; <=0=>max+i; >0=>capped by max
//                           (use_threads is a deprecated alias)
//   max_async{n}            0=>follow jobs width; >0=>absolute live-proc cap
//   deduce_dependency{}     span<Task>: build DAG from Task inputs/outputs/after
//   keep_going{}            run all possible despite failures (default: stop)
//   dry_run{}               resolve + print, spawn nothing (composes with force)
//   force{}                 ignore up-to-date, re-run everything
//   write_compile_commands{p}  emit a compile_commands.json
//   Rules: <=1 of each. io here FAILS TO COMPILE on purpose —
//   it is per-Task. There is no run(span<Proc>, ...): use wait_all.
//
// Sections 1-6 succeed; 7-8 demo every config error; 9 lists compile errors.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

#include <any>

namespace {

auto task_state_name(bld::Task_state s) -> const char *
{
    switch (s) {
    case bld::Task_state::pending:
        return "pending";
    case bld::Task_state::running:
        return "running";
    case bld::Task_state::skipped:
        return "skipped";
    case bld::Task_state::succeeded:
        return "succeeded";
    case bld::Task_state::failed:
        return "failed";
    case bld::Task_state::cancelled:
        return "cancelled";
    }
    return "?";
}

auto show_result(const char *tag, std::expected<bld::Run_result, bld::Err> &res) -> void
{
    if (!res) {
        bld::log::e("{} failed: {}", tag, res.error());
        if (auto *r = std::any_cast<bld::Run_result>(&res.error().payload)) {
            for (std::size_t i = 0; i < r->tasks.size(); ++i) {
                const auto &t = r->tasks[i];
                if (t.state == bld::Task_state::failed) {
                    bld::log::e("  task[{}] state={} msg={}", i, task_state_name(t.state), t.message);
                }
            }
        }
    } else {
        bld::log::i("{} ran={} skipped={} failed={}", tag, res->ran, res->skipped, res->failed);
        for (auto &t : res->tasks) {
            bld::log::i("  task state={} msg='{}'", task_state_name(t.state), t.message);
        }
    }
}

auto expect_err(const char *tag, std::expected<bld::Run_result, bld::Err> &res, std::string_view want) -> void
{
    if (res) {
        bld::log::e("{} UNEXPECTED SUCCESS (wanted Err containing '{}')", tag, want);
    } else if (res.error().msg.find(want) == std::string::npos) {
        bld::log::e("{} wrong error: '{}' (wanted '{}')", tag, res.error().msg, want);
    } else {
        bld::log::i("{} correctly failed: {}", tag, res.error().msg);
    }
}

auto expect_proc_err(const char *tag, std::expected<bld::Proc, bld::Err> &res, std::string_view want) -> void
{
    if (res) {
        bld::log::e("{} UNEXPECTED SUCCESS (wanted Err containing '{}')", tag, want);
    } else if (res.error().msg.find(want) == std::string::npos) {
        bld::log::e("{} wrong error: '{}' (wanted '{}')", tag, res.error().msg, want);
    } else {
        bld::log::i("{} correctly failed: {}", tag, res.error().msg);
    }
}

} // namespace

int main()
{
    bld::fs::make_dirs("demo_build").value(); // ensure output dir exists

    // 1. Single command — sync, returns Proc.
    {
        auto proc = bld::run(bld::Cmd{"echo", "hi"});
        if (!proc) {
            bld::log::e("1 failed: {}", proc.error());
        } else {
            bld::log::i("1: echo exited code={} label={}", proc->status_code(), proc->spec.cfg.label);
        }
    }

    // 1b. label + cwd — run in a directory without chdir in the parent.
    {
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "pwd"}, bld::label{"pwd-test"}, bld::cwd{"demo_build"}); !proc) {
            bld::log::e("1b failed: {}", proc.error());
        }
    }

    // 1c. Every output routing: fd (borrowed), file (shared), lazy (path), merged.
    {
        // out_fd borrows a raw fd; err goes to a lazy path file.
        auto owned = bld::Owned_Fd::open("demo_build/1c_out.txt", bld::Open_mode::write).value();
        if (auto proc = bld::run(
                bld::Cmd{"sh", "-c", "echo hello; echo err >&2"},
                bld::label{"with-fd"},
                bld::out_fd{owned},
                bld::lazy_err_file{"demo_build/1c_err.txt"});
            !proc) {
            bld::log::e("1c fd+lazy failed: {}", proc.error());
        }

        // out_file opens eagerly (Shared_fd keeps it alive through the run).
        if (auto f = bld::out_file::open("demo_build/1c_eager.txt"); !f) {
            bld::log::e("1c eager open failed: {}", f.error());
        } else if (auto proc = bld::run(bld::Cmd{"echo", "eager"}, *f); !proc) {
            bld::log::e("1c eager run failed: {}", proc.error());
        }

        // out_err_file merges out+err into one eager file.
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo o; echo e >&2"}, bld::out_err_file{"demo_build/1c_both.txt"}); !proc) {
            bld::log::e("1c merged file failed: {}", proc.error());
        }
        // lazy_out_err_file is the path (lazy) version of the same.
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo o; echo e >&2"}, bld::lazy_out_err_file{"demo_build/1c_both_lazy.txt"});
            !proc) {
            bld::log::e("1c merged lazy failed: {}", proc.error());
        }
        // out_err_fd is the borrowed-fd version: one fd gets both streams.
        {
            auto both = bld::Owned_Fd::open("demo_build/1c_both_fd.txt", bld::Open_mode::write).value();
            if (auto proc = bld::run(
                    bld::Cmd{"sh", "-c", "echo o; echo e >&2"}, bld::out_err_fd{both});
                !proc) {
                bld::log::e("1c merged fd failed: {}", proc.error());
            }
        }
    }

    // 1d. Every input routing: in_fd, in_file, lazy_in_file.
    {
        bld::fs::write_file("demo_build/in.txt", "from file\n").value();
        if (auto f = bld::in_file::open("demo_build/in.txt"); !f) {
            bld::log::e("1d in_file open failed: {}", f.error());
        } else if (auto proc = bld::run(bld::Cmd{"cat"}, *f); !proc) {
            bld::log::e("1d in_file failed: {}", proc.error());
        }
        if (auto proc = bld::run(bld::Cmd{"cat"}, bld::lazy_in_file{"demo_build/in.txt"}); !proc) {
            bld::log::e("1d lazy_in failed: {}", proc.error());
        }
        auto owned = bld::Owned_Fd::open("demo_build/in.txt", bld::Open_mode::read).value();
        if (auto proc = bld::run(bld::Cmd{"cat"}, bld::in_fd{owned}); !proc) {
            bld::log::e("1d in_fd failed: {}", proc.error());
        }
    }

    // 1e. pipe — monolithic routing; merge_err_and_out folds stderr into stdout.
    {
        if (auto proc = bld::run(bld::Cmd{"echo", "piped"}, bld::pipe{.out = bld::Fd_view{STDOUT_FILENO}}); !proc) {
            bld::log::e("1e pipe failed: {}", proc.error());
        }
        if (auto proc = bld::run(
                bld::Cmd{"sh", "-c", "echo o; echo e >&2"},
                bld::pipe{.out = bld::Fd_view{STDOUT_FILENO}, .merge_err_and_out = true});
            !proc) {
            bld::log::e("1e merged pipe failed: {}", proc.error());
        }
    }

    // 1f. async — returns a running Proc; caller waits/kills. wait_all reaps many.
    {
        auto p1 = bld::run(bld::Cmd{"sleep", "0.1"}, bld::async{});
        auto p2 = bld::run(bld::Cmd{"sleep", "0.1"}, bld::async{});
        if (!p1 || !p2) {
            bld::log::e("1f spawn failed");
        } else {
            std::vector<bld::Proc> procs;
            procs.push_back(std::move(*p1));
            procs.push_back(std::move(*p2));
            if (auto waited = bld::wait_all(procs); !waited) {
                bld::log::e("1f wait_all failed: {}", waited.error());
            } else {
                bld::log::i("1f wait_all ok {}", *waited);
            }
        }
        // wait_all with a failing proc reports an error (with count payload).
        {
            auto ok = bld::run(bld::Cmd{"true"}, bld::async{});
            auto bad = bld::run(bld::Cmd{"false"}, bld::async{});
            std::vector<bld::Proc> procs;
            procs.push_back(std::move(*ok));
            procs.push_back(std::move(*bad));
            if (auto waited = bld::wait_all(procs); waited) {
                bld::log::e("1f failing wait_all UNEXPECTED SUCCESS");
            } else {
                bld::log::i("1f failing wait_all correctly failed: {}", waited.error());
            }
        }
    }

    // 1g. Proc::kill on a runaway, then wait for the signaled status.
    {
        auto runaway = bld::run(bld::Cmd{"sleep", "30"}, bld::async{});
        if (!runaway) {
            bld::log::e("1g spawn failed: {}", runaway.error());
        } else {
            runaway->kill();
            if (auto st = runaway->wait(); !st) {
                bld::log::e("1g wait failed: {}", st.error());
            } else {
                bld::log::i("1g killed, state={} code={}", static_cast<int>(st->state), st->code);
            }
        }
    }

    // 1h. out_str / err_str / out_err_str — capture into strings via run().
    //    The strings are borrowed: they must outlive wait()/reap. Output stays
    //    separate by default; only out_err_str merges (like out_err_file).
    {
        std::string out, err;
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo to-stdout && echo to-stderr >&2"}, bld::out_str{out}, bld::err_str{err});
            !proc) {
            bld::log::e("1h split failed: {}", proc.error());
        } else {
            bld::log::i("1h split out='{}' err='{}'", bld::str::trim(out), bld::str::trim(err));
        }

        std::string merged;
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo a && echo b >&2"}, bld::out_err_str{merged}); !proc) {
            bld::log::e("1h merged failed: {}", proc.error());
        } else {
            bld::log::i("1h merged='{}'", bld::str::trim(merged));
        }

        // Works detached too: capture pipes live in the Proc, pumped on wait().
        std::string late;
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "sleep 0.1; echo late"}, bld::async{}, bld::out_str{late})) {
            std::ignore = proc->wait();
            bld::log::i("1h async out='{}'", bld::str::trim(late));
        }

        // Works per-Task in batch runs (scheduler reaps via wait_any, same pumps).
        {
            std::string bout;
            std::vector<bld::Task> tasks;
            tasks.emplace_back(bld::Cmd{"echo", "batched"}, bld::out_str{bout});
            if (auto res = bld::run(tasks, bld::jobs{2}); !res) {
                bld::log::e("1h batch failed: {}", res.error());
            } else {
                bld::log::i("1h batch out='{}'", bld::str::trim(bout));
            }
        }

        // 1i. dry_run previews a single command or capture: logs only, spawns
        // nothing (even `false` reports success, capture comes back empty).
        if (auto proc = bld::run(bld::Cmd{"false"}, bld::dry_run{}); !proc || proc->status_code() != 0) {
            bld::log::e("1i single dry-run failed");
        }
        if (auto out = bld::capture(bld::Cmd{"echo", "hi"}, bld::dry_run{}); !out || !out->empty()) {
            bld::log::e("1i capture dry-run failed");
        } else {
            bld::log::i("1i dry-runs ok (nothing spawned)");
        }
    }

    // 2. span<Task> run-all: every task runs, no graph.
    {
        // Bare defaults: jobs nullopt (=> max-1), async follows jobs width.
        std::vector<bld::Task> tasks;
        tasks.emplace_back(bld::Cmd{"echo", "a"});
        tasks.emplace_back(bld::Cmd{"echo", "b"});
        auto res = bld::run(tasks);
        show_result("2a defaults", res);
    }

    // 2b. jobs forms: width caps live procs, max_async caps them too.
    {
        bld::log::i(
            "2b max={} nullopt={} {{-1}}={} {{0}}={} {{2}}={} huge={}",
            bld::max_parallel_count(),
            bld::resolve_parallel_width(std::nullopt),
            bld::resolve_parallel_width(-1),
            bld::resolve_parallel_width(0),
            bld::resolve_parallel_width(2),
            bld::resolve_parallel_width(1000000));
        std::vector<bld::Task> tasks;
        tasks.emplace_back(bld::Cmd{"echo", "a"});
        tasks.emplace_back(bld::Cmd{"echo", "b"});
        auto r1 = bld::run(tasks, bld::jobs{2});
        show_result("2b jobs{2}", r1);
        auto r2 = bld::run(tasks, bld::jobs{0}); // 0 => max
        show_result("2b jobs{0}=max", r2);
        auto r3 = bld::run(tasks, bld::jobs{}, bld::max_async{1}); // serialize procs
        show_result("2b async{1}", r3);
        auto r4 = bld::run(tasks, bld::jobs{2}, bld::max_async{8}); // cap above jobs: jobs win
        show_result("2b async{8}", r4);
    }

    // 2c. Per-task config inside a batch: label/cwd/io each live on the Task.
    {
        std::vector<bld::Task> tasks;
        tasks.emplace_back(bld::Cmd{"sh", "-c", "pwd"}, bld::label{"pwd-task"}, bld::cwd{"demo_build"});
        tasks.emplace_back(bld::Cmd{"sh", "-c", "echo hi > demo_build/2c.txt"}, bld::lazy_out_file{"demo_build/2c_extra.txt"});
        auto res = bld::run(tasks, bld::jobs{4}, bld::keep_going{});
        show_result("2c per-task config", res);
    }

    // 2d. keep_going vs stop (default): poison pill shows who gets scheduled.
    {
        std::vector<bld::Task> fragile;
        fragile.emplace_back(bld::Cmd{"true"});
        fragile.emplace_back(bld::Cmd{"false"});
        fragile.emplace_back(bld::Cmd{"true"});
        auto stopped = bld::run(fragile, bld::jobs{1});
        show_result("2d stop", stopped); // 3rd task cancelled
        auto going = bld::run(fragile, bld::jobs{1}, bld::keep_going{});
        show_result("2d keep_going", going); // 3rd task ran
    }

    // 2e. dry_run previews (spawns nothing); force re-runs up-to-date work.
    {
        std::vector<bld::Task> tasks;
        bld::Task a{bld::Cmd{"sh", "-c", "echo a > demo_build/2e.o"}};
        a.name = "e_a";
        a.produces("demo_build/2e.o");
        tasks.push_back(std::move(a));
        auto first = bld::run(tasks, bld::jobs{2}, bld::deduce_dependency{});
        show_result("2e first", first); // ran
        auto second = bld::run(tasks, bld::jobs{2}, bld::deduce_dependency{});
        show_result("2e second", second); // skipped: up to date
        auto preview = bld::run(tasks, bld::jobs{2}, bld::deduce_dependency{}, bld::force{}, bld::dry_run{});
        show_result("2e force+dry_run preview", preview); // dry run (would re-run)
        auto forced = bld::run(tasks, bld::jobs{2}, bld::deduce_dependency{}, bld::force{});
        show_result("2e forced", forced); // ran again
    }

    // 2f. write_compile_commands on a span writes EVERY task (no marking there).
    {
        std::vector<bld::Task> tasks;
        tasks.emplace_back(bld::Cmd{"g++", "-c", "a.cpp", "-o", "a.o"});
        tasks.emplace_back(bld::Cmd{"echo", "not-a-compile"});
        auto res = bld::run(tasks, bld::dry_run{}, bld::write_compile_commands{"demo_build/span_cc.json"});
        show_result("2f span db (dry)", res);
        if (auto db = bld::fs::read_file("demo_build/span_cc.json")) {
            bld::log::i("2f span db bytes={} (both tasks written)", db->size());
        }
    }

    // 2g. Empty span is ok and runs nothing.
    {
        std::vector<bld::Task> tasks;
        auto res = bld::run(tasks);
        show_result("2g empty", res);
    }

    // 3. span<Task> + deduce_dependency: Task builders form the DAG.
    {
        // Fresh outputs so the first run really runs the whole chain.
        std::filesystem::remove("demo_build/d_a.o");
        std::filesystem::remove("demo_build/d_b.o");
        std::filesystem::remove("demo_build/d_c.o");
        std::vector<bld::Task> tasks;
        bld::Task a{bld::Cmd{"sh", "-c", "echo a > demo_build/d_a.o"}};
        a.name = "d_a";
        a.produces("demo_build/d_a.o");
        bld::Task b{bld::Cmd{"sh", "-c", "cat demo_build/d_a.o > demo_build/d_b.o"}};
        b.name = "d_b";
        b.needs("demo_build/d_a.o").produces("demo_build/d_b.o");
        bld::Task c{bld::Cmd{"sh", "-c", "cat demo_build/d_b.o > demo_build/d_c.o"}};
        c.name = "d_c";
        c.needs_from({"demo_build/d_b.o"}).produces_to({"demo_build/d_c.o"}).after_dep("d_a");
        tasks.push_back(std::move(a));
        tasks.push_back(std::move(b));
        tasks.push_back(std::move(c));
        auto res = bld::run(tasks, bld::jobs{2}, bld::deduce_dependency{});
        show_result("3 deduce chain", res);
        auto again = bld::run(tasks, bld::jobs{2}, bld::deduce_dependency{});
        show_result("3 deduce up-to-date", again); // all skipped
    }

    // 4. Plan — graph + outdated lives in Plan maps; same Run modifiers as span.
    {
        bld::Plan plan;
        plan.add("a", bld::Cmd{"sh", "-c", "echo a > demo_build/a.o"});
        plan.needs("a", "examples/run.cpp");
        plan.produces("a", "demo_build/a.o");
        plan.mark_compile_command("a");
        plan.add("b", bld::Cmd{"sh", "-c", "echo b > demo_build/b.o"});
        plan.produces("b", "demo_build/b.o");
        plan.add("app", bld::Cmd{"sh", "-c", "cat demo_build/a.o demo_build/b.o > demo_build/app"});
        plan.needs_from("app", {"demo_build/a.o", "demo_build/b.o"});
        plan.produces("app", "demo_build/app");
        plan.after("app", "a");
        plan.add("extra", bld::Cmd{"echo", "extra"});
        plan.needs("extra", "demo_build/app");
        plan.produces_to("extra", {"demo_build/extra1", "demo_build/extra2"});

        auto res = bld::run(plan, bld::jobs{4}, bld::write_compile_commands{"demo_build/compile_commands.json"});
        show_result("4 plan", res); // only mark_compile_command("a") lands in the db
        if (auto db = bld::fs::read_file("demo_build/compile_commands.json")) {
            const bool has_a = db->find("demo_build/a.o") != std::string::npos;
            const bool has_b = db->find("echo b") != std::string::npos;
            bld::log::i("4 plan db has_a_output={} has_unmarked_b={} (want true/false)", has_a, has_b);
        }
        auto dry = bld::run(plan, bld::dry_run{});
        show_result("4 plan dry", dry);
        auto forced = bld::run(plan, bld::force{}, bld::jobs{2});
        show_result("4 plan forced", forced);
        auto capped = bld::run(plan, bld::jobs{}, bld::max_async{4});
        show_result("4 plan capped", capped);
    }

    // 5. Compilation database — load, then run like any span.
    {
        if (!bld::fs::exists("demo_build/compile_commands.json")) {
            bld::log::w("5: no compile_commands.json yet — run section 4 first");
        } else {
            auto res = bld::run(bld::compile_commands("demo_build/compile_commands.json"), bld::jobs{4});
            show_result("5a db", res);
            auto res2 = bld::run(
                bld::compile_commands("demo_build/compile_commands.json", true), bld::jobs{4}, bld::keep_going{});
            show_result("5b db infer+keep_going", res2);
            // infer_outputs parses "-o <file>" into Task.outputs; false leaves them empty.
            // The Plan-written db above already carries explicit "output" keys, so
            // craft one entry without it to show the difference.
            bld::fs::write_file(
                "demo_build/infer_cc.json",
                "[{\"directory\": \".\", \"file\": \"a.cpp\", \"arguments\": [\"g++\", \"-c\", \"a.cpp\", \"-o\", \"a.o\"]}]")
                .value();
            auto with = bld::details::load_compile_commands(bld::compile_commands("demo_build/infer_cc.json", true));
            auto without = bld::details::load_compile_commands(bld::compile_commands("demo_build/infer_cc.json", false));
            if (with && without) {
                const bool w = !with->empty() && !(*with)[0].outputs.empty();
                const bool wo = !without->empty() && !(*without)[0].outputs.empty();
                bld::log::i("5c infer outputs: true_has={} false_has={} (want true/false)", w, wo);
                if (w) {
                    bld::log::i("5c inferred output='{}' input='{}'", (*with)[0].outputs.front(), (*with)[0].inputs.front());
                }
            }
            // Missing file is an error, not a crash.
            auto missing = bld::run(bld::compile_commands("demo_build/nope.json"));
            expect_err("5d missing db", missing, "nope.json");
        }
    }

    // 6. wait_all — reaps detached async Procs; takes NO modifiers.
    {
        auto p1 = bld::run(bld::Cmd{"sleep", "0.1"}, bld::async{});
        auto p2 = bld::run(bld::Cmd{"sleep", "0.1"}, bld::async{});
        if (!p1 || !p2) {
            bld::log::e("6 spawn failed");
        } else {
            std::vector<bld::Proc> procs;
            procs.push_back(std::move(*p1));
            procs.push_back(std::move(*p2));
            if (auto waited = bld::wait_all(procs); !waited) {
                bld::log::e("6 wait_all failed: {}", waited.error());
            } else {
                bld::log::i("6 wait_all ok {}", *waited);
            }
        }
    }

    // 7. Runtime config errors — each prints the Err the library returns.
    {
        // 7a. Task deps without deduce_dependency: would silently mis-order.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"echo", "a"}};
            a.name = "a";
            a.produces("demo_build/7a.o");
            bld::Task b{bld::Cmd{"echo", "b"}};
            b.name = "b";
            b.needs("demo_build/7a.o");
            tasks.push_back(std::move(a));
            tasks.push_back(std::move(b));
            auto res = bld::run(tasks);
            expect_err("7a deps-without-deduce", res, "deduce_dependency");
        }
        // 7b. deduce_dependency with a Plan: Plans always graph already.
        {
            bld::Plan plan;
            plan.add("x", bld::Cmd{"echo", "x"});
            auto res = bld::run(plan, bld::deduce_dependency{});
            expect_err("7b plan+deduce", res, "deduce_dependency");
        }
        // 7c. Empty command names the task.
        {
            std::vector<bld::Task> tasks;
            bld::Task t;
            t.name = "oops-empty";
            tasks.push_back(std::move(t));
            auto res = bld::run(tasks);
            expect_err("7c empty cmd", res, "oops-empty");
        }
        // 7d. Duplicate task names.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"true"}};
            a.name = "dup";
            bld::Task b{bld::Cmd{"true"}};
            b.name = "dup";
            tasks.push_back(std::move(a));
            tasks.push_back(std::move(b));
            auto res = bld::run(tasks, bld::deduce_dependency{});
            expect_err("7d dup name", res, "duplicate task name 'dup'");
        }
        // 7e. Duplicate outputs.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"true"}};
            a.name = "a";
            a.produces("demo_build/7e.o");
            bld::Task b{bld::Cmd{"true"}};
            b.name = "b";
            b.produces("demo_build/7e.o");
            tasks.push_back(std::move(a));
            tasks.push_back(std::move(b));
            auto res = bld::run(tasks, bld::deduce_dependency{});
            expect_err("7e dup output", res, "multiple tasks produce");
        }
        // 7f. Unknown after-dependency.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"true"}};
            a.name = "a";
            a.after_dep("ghost");
            tasks.push_back(std::move(a));
            auto res = bld::run(tasks, bld::deduce_dependency{});
            expect_err("7f unknown dep", res, "unknown task 'ghost'");
        }
        // 7g. Self-dependency.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"true"}};
            a.name = "self";
            a.after_dep("self");
            tasks.push_back(std::move(a));
            auto res = bld::run(tasks, bld::deduce_dependency{});
            expect_err("7g self dep", res, "depends on itself");
        }
        // 7h. Cycle a<->b.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"true"}};
            a.name = "a";
            a.produces("demo_build/7h_a");
            a.needs("demo_build/7h_b");
            bld::Task b{bld::Cmd{"true"}};
            b.name = "b";
            b.produces("demo_build/7h_b");
            b.needs("demo_build/7h_a");
            tasks.push_back(std::move(a));
            tasks.push_back(std::move(b));
            auto res = bld::run(tasks, bld::deduce_dependency{});
            expect_err("7h cycle", res, "cycle");
        }
        // 7i. Missing cwd.
        {
            auto res = bld::run(bld::Cmd{"true"}, bld::cwd{"demo_build/nope"});
            expect_proc_err("7i missing cwd", res, "does not exist");
        }
        // 7j. Eager open of a missing file (lazy form fails at spawn instead).
        {
            auto f = bld::in_file::open("demo_build/nope.txt");
            if (!f) {
                bld::log::i("7j eager open correctly failed: {}", f.error());
            } else {
                bld::log::e("7j eager open UNEXPECTED SUCCESS");
            }
            auto res = bld::run(bld::Cmd{"cat"}, bld::lazy_in_file{"demo_build/nope.txt"});
            expect_proc_err("7j lazy open at spawn", res, "nope.txt");
        }
        // 7k. Plan unknown after-dependency.
        {
            bld::Plan plan;
            plan.add("a", bld::Cmd{"true"});
            plan.after("a", "ghost");
            auto res = bld::run(plan);
            expect_err("7k plan unknown dep", res, "unknown task 'ghost'");
        }
    }

    // 8. Compile-time errors (uncomment to see the guided messages; none build).
    //    Each fires a static_assert naming the right modifier set.
    //   bld::run(tasks, bld::out_fd{...});            // io routing is per-Task, not a Run modifier
    //   bld::run(cmd, bld::jobs{2});           // jobs is a Run modifier, not per-process
    //   bld::capture(cmd, bld::out_fd{...});          // capture has no out_* modifiers (merged string)
    //   tasks.emplace_back(cmd, bld::jobs{2}); // Task takes Proc modifiers, not Run modifiers
    //   grp.run_new(cmd, bld::keep_going{});          // run_new takes Proc modifiers, not Run modifiers
    //   bld::run(tasks, bld::jobs{2}, bld::jobs{3}); // duplicate flag
    //   bld::run(cmd, bld::label{"a"}, bld::label{"b"});           // duplicate label
    //   bld::run(cmd, bld::cwd{"a"}, bld::cwd{"b"});               // duplicate cwd
    //   bld::run(cmd, bld::async{}, bld::async{});                 // duplicate async
    //   bld::run(cmd, bld::out_fd{f}, bld::out_file{...});         // two out routes
    //   bld::run(cmd, bld::out_str{s}, bld::out_file{...});         // two out routes (str counts too)
    //   bld::run(cmd, bld::out_err_fd{f}, bld::err_file{...});      // out_err_fd counts as err too
    //   bld::run(cmd, bld::pipe{...}, bld::out_fd{f});             // pipe mixes with nothing
    //   bld::run(cmd, bld::pipe{...}, bld::out_str{s});            // pipe mixes with nothing (str too)
    //   bld::run(cmd, bld::in_str{"hi"});             // in_str is capture-only
    //   bld::wait_all(procs, bld::keep_going{});      // wait_all takes no modifiers

    return 0;
}
