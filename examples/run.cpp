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
//   io_in{...}              stdin: borrowed fd, lazy path, or eager io_in::open
//   in_str{text}            stdin content: fed synchronously, then EOF
//                           (empty behaves like unset, like capture())
//   io_out{...}             stdout: borrowed fd, lazy path, eager io_out::open,
//                           or capture string via io_out{&s} (borrowed,
//                           must outlive wait)
//   io_err{...}             stderr: same shapes as io_out
//   io_out_err{...}            one route for merged stdout+stderr (implies merging)
//   Rules: <=1 of each; io_out_err conflicts with io_out/io_err; io_in conflicts
//   with in_str.
//
// RUN MODIFIERS (whole batch only — Run_modifier_c, consume Run_config):
//   jobs{[opt]int}         nullopt=>max-1; value=>exactly that, clamped [1, max]
//   max_async{n}            0=>follow jobs width; >0=>absolute live-proc cap
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
    std::ignore = bld::fs::make_dirs("demo_build"); // ensure output dir exists

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

    // 1c. Every output routing: borrowed fd, lazy path, eager open, merged.
    {
        // io_out borrows a raw fd; err goes to a lazy path file.
        auto owned = bld::Owned_Fd::open("demo_build/1c_out.txt", bld::Open_mode::write).value();
        if (auto proc = bld::run(
                bld::Cmd{"sh", "-c", "echo hello; echo err >&2"},
                bld::label{"with-fd"},
                bld::io_out{owned},
                bld::io_err{"demo_build/1c_err.txt"});
            !proc) {
            bld::log::e("1c fd+lazy failed: {}", proc.error());
        }

        // io_out::open opens eagerly (the io value keeps the fd alive).
        if (auto f = bld::io_out::open("demo_build/1c_eager.txt"); !f) {
            bld::log::e("1c eager open failed: {}", f.error());
        } else if (auto proc = bld::run(bld::Cmd{"echo", "eager"}, *f); !proc) {
            bld::log::e("1c eager run failed: {}", proc.error());
        }

        // io_out_err merges out+err into one file (lazy path version).
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo o; echo e >&2"}, bld::io_out_err{"demo_build/1c_both.txt"}); !proc) {
            bld::log::e("1c merged file failed: {}", proc.error());
        }
        // io_out_err from an eager open.
        if (auto f = bld::io_out_err::open("demo_build/1c_both_eager.txt"); !f) {
            bld::log::e("1c merged eager open failed: {}", f.error());
        } else if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo o; echo e >&2"}, *f); !proc) {
            bld::log::e("1c merged eager failed: {}", proc.error());
        }
        // io_out_err from a borrowed fd: one fd gets both streams.
        {
            auto both = bld::Owned_Fd::open("demo_build/1c_both_fd.txt", bld::Open_mode::write).value();
            if (auto proc = bld::run(
                    bld::Cmd{"sh", "-c", "echo o; echo e >&2"}, bld::io_out_err{both});
                !proc) {
                bld::log::e("1c merged fd failed: {}", proc.error());
            }
        }
    }

    // 1d. Every input routing: eager open, lazy path, borrowed fd, string content.
    {
        std::ignore = bld::fs::write_file("demo_build/in.txt", "from file\n");
        if (auto f = bld::io_in::open("demo_build/in.txt"); !f) {
            bld::log::e("1d io_in open failed: {}", f.error());
        } else if (auto proc = bld::run(bld::Cmd{"cat"}, *f); !proc) {
            bld::log::e("1d io_in failed: {}", proc.error());
        }
        if (auto proc = bld::run(bld::Cmd{"cat"}, bld::io_in{"demo_build/in.txt"}); !proc) {
            bld::log::e("1d lazy_in failed: {}", proc.error());
        }
        auto owned = bld::Owned_Fd::open("demo_build/in.txt", bld::Open_mode::read).value();
        if (auto proc = bld::run(bld::Cmd{"cat"}, bld::io_in{owned}); !proc) {
            bld::log::e("1d in_fd failed: {}", proc.error());
        }
        // in_str feeds literal content (fed synchronously, then EOF).
        std::string from_str;
        if (auto proc = bld::run(bld::Cmd{"cat"}, bld::in_str{"from string\n"}, bld::io_out{&from_str}); !proc) {
            bld::log::e("1d in_str failed: {}", proc.error());
        } else {
            bld::log::i("1d in_str out='{}'", bld::str::trim(from_str));
        }
    }

    // 1e. Direct fd routing; io_out_err folds stderr into stdout.
    {
        if (auto proc = bld::run(bld::Cmd{"echo", "piped"}, bld::io_out{bld::Fd_view{STDOUT_FILENO}}); !proc) {
            bld::log::e("1e io_out failed: {}", proc.error());
        }
        if (auto proc = bld::run(
                bld::Cmd{"sh", "-c", "echo o; echo e >&2"},
                bld::io_out_err{bld::Fd_view{STDOUT_FILENO}});
            !proc) {
            bld::log::e("1e merged io_out_err failed: {}", proc.error());
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
        // wait_all with a failing proc reports an error.
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

    // 1h. io_out{&s} / io_err{&s} / io_out_err{&s} — capture into strings via run().
    //    The strings are borrowed: they must outlive wait()/reap. Output stays
    //    separate by default; only io_out_err merges.
    {
        std::string out, err;
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo to-stdout && echo to-stderr >&2"}, bld::io_out{&out}, bld::io_err{&err});
            !proc) {
            bld::log::e("1h split failed: {}", proc.error());
        } else {
            bld::log::i("1h split out='{}' err='{}'", bld::str::trim(out), bld::str::trim(err));
        }

        std::string merged;
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "echo a && echo b >&2"}, bld::io_out_err{&merged}); !proc) {
            bld::log::e("1h merged failed: {}", proc.error());
        } else {
            bld::log::i("1h merged='{}'", bld::str::trim(merged));
        }

        // Works detached too: capture pipes live in the Proc, pumped on wait().
        std::string late;
        if (auto proc = bld::run(bld::Cmd{"sh", "-c", "sleep 0.1; echo late"}, bld::async{}, bld::io_out{&late})) {
            std::ignore = proc->wait();
            bld::log::i("1h async out='{}'", bld::str::trim(late));
        }

        // Works per-Task in batch runs (scheduler reaps via wait_any, same pumps).
        {
            std::string bout;
            std::vector<bld::Task> tasks;
            tasks.emplace_back(bld::Cmd{"echo", "batched"}, bld::io_out{&bout});
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
            "2b max={} nullopt={} {{0}}={} {{2}}={} huge={}",
            bld::max_parallel_count(),
            bld::resolve_parallel_width(std::nullopt),
            bld::resolve_parallel_width(0),
            bld::resolve_parallel_width(2),
            bld::resolve_parallel_width(1000000));
        std::vector<bld::Task> tasks;
        tasks.emplace_back(bld::Cmd{"echo", "a"});
        tasks.emplace_back(bld::Cmd{"echo", "b"});
        auto r1 = bld::run(tasks, bld::jobs{2});
        show_result("2b jobs{2}", r1);
        auto r2 = bld::run(tasks, bld::jobs{0}); // 0 clamps to 1: serial
        show_result("2b jobs{0}=1", r2);
        auto r3 = bld::run(tasks, bld::jobs{}, bld::max_async{1}); // serialize procs
        show_result("2b async{1}", r3);
        auto r4 = bld::run(tasks, bld::jobs{2}, bld::max_async{8}); // cap above jobs: jobs win
        show_result("2b async{8}", r4);
    }

    // 2c. Per-task config inside a batch: label/cwd/io each live on the Task.
    {
        std::vector<bld::Task> tasks;
        tasks.emplace_back(bld::Cmd{"sh", "-c", "pwd"}, bld::label{"pwd-task"}, bld::cwd{"demo_build"});
        tasks.emplace_back(bld::Cmd{"sh", "-c", "echo hi > demo_build/2c.txt"}, bld::io_out{"demo_build/2c_extra.txt"});
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
    // A one-task Plan (span<Task> never skips — ordering lives in Plan).
    {
        auto build = [] {
            bld::Plan plan;
            plan.add("e_a", bld::Cmd{"sh", "-c", "echo a > demo_build/2e.o"});
            plan.produces("e_a", "demo_build/2e.o");
            return plan;
        };
        bld::Plan p1 = build();
        auto first = bld::run(p1, bld::jobs{2});
        show_result("2e first", first); // ran
        bld::Plan p2 = build();
        auto second = bld::run(p2, bld::jobs{2});
        show_result("2e second", second); // skipped: up to date
        bld::Plan p3 = build();
        auto preview = bld::run(p3, bld::jobs{2}, bld::force{}, bld::dry_run{});
        show_result("2e force+dry_run preview", preview); // dry run (would re-run)
        bld::Plan p4 = build();
        auto forced = bld::run(p4, bld::jobs{2}, bld::force{});
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

    // 3. Plan chain: Task builders form the DAG in Plan maps.
    {
        // Fresh outputs so the first run really runs the whole chain.
        std::filesystem::remove("demo_build/d_a.o");
        std::filesystem::remove("demo_build/d_b.o");
        std::filesystem::remove("demo_build/d_c.o");
        auto build = [] {
            bld::Plan plan;
            plan.add("d_a", bld::Cmd{"sh", "-c", "echo a > demo_build/d_a.o"});
            plan.produces("d_a", "demo_build/d_a.o");
            plan.add("d_b", bld::Cmd{"sh", "-c", "cat demo_build/d_a.o > demo_build/d_b.o"});
            plan.needs("d_b", "demo_build/d_a.o");
            plan.produces("d_b", "demo_build/d_b.o");
            plan.add("d_c", bld::Cmd{"sh", "-c", "cat demo_build/d_b.o > demo_build/d_c.o"});
            plan.needs("d_c", "demo_build/d_b.o");
            plan.produces("d_c", "demo_build/d_c.o");
            plan.after("d_c", "d_a");
            return plan;
        };
        bld::Plan plan = build();
        auto res = bld::run(plan, bld::jobs{2});
        show_result("3 plan chain", res);
        bld::Plan plan2 = build();
        auto again = bld::run(plan2, bld::jobs{2});
        show_result("3 plan up-to-date", again); // all skipped
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
            // infer_outputs parses "-o <file>" into Plan outputs; false leaves them empty.
            // The Plan-written db above already carries explicit "output" keys, so
            // craft one entry without it to show the difference.
            std::ignore = bld::fs::write_file(
                "demo_build/infer_cc.json",
                "[{\"directory\": \".\", \"file\": \"a.cpp\", \"arguments\": [\"g++\", \"-c\", \"a.cpp\", \"-o\", \"a.o\"]}]");
            auto with = bld::details::load_compile_commands(bld::compile_commands("demo_build/infer_cc.json", true));
            auto without = bld::details::load_compile_commands(bld::compile_commands("demo_build/infer_cc.json", false));
            if (with && without) {
                auto has_out = [](const auto &plan) {
                    auto it = plan.task_outputs.find("a.cpp");
                    return it != plan.task_outputs.end() && !it->second.empty();
                };
                const bool w = !with->tasks.empty() && has_out(*with);
                const bool wo = !without->tasks.empty() && has_out(*without);
                bld::log::i("5c infer outputs: true_has={} false_has={} (want true/false)", w, wo);
                if (w) {
                    bld::log::i(
                        "5c inferred output='{}' input='{}'",
                        with->task_outputs["a.cpp"].front(),
                        with->task_inputs["a.cpp"].front());
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
        // 7a. Declared deps order automatically in a Plan (span<Task> is
        // unordered by design — it holds no dependency info at all).
        {
            bld::Plan plan;
            plan.add("a", bld::Cmd{"echo", "a"});
            plan.produces("a", "demo_build/7a.o");
            plan.add("b", bld::Cmd{"echo", "b"});
            plan.needs("b", "demo_build/7a.o");
            auto res = bld::run(plan);
            show_result("7a plan graph", res); // ran=2, ordered a before b
        }
        // 7b. Empty command names the task.
        {
            std::vector<bld::Task> tasks;
            bld::Task t;
            t.name = "empty-cmd";
            tasks.push_back(std::move(t));
            auto res = bld::run(tasks);
            expect_err("7b empty cmd", res, "empty-cmd");
        }
        // 7c. Duplicate task names.
        {
            std::vector<bld::Task> tasks;
            bld::Task a{bld::Cmd{"true"}};
            a.name = "dup";
            bld::Task b{bld::Cmd{"true"}};
            b.name = "dup";
            tasks.push_back(std::move(a));
            tasks.push_back(std::move(b));
            auto res = bld::run(tasks);
            expect_err("7c dup name", res, "duplicate task name 'dup'");
        }
        // 7d. Duplicate outputs.
        {
            bld::Plan plan;
            plan.add("a", bld::Cmd{"true"});
            plan.produces("a", "demo_build/7e.o");
            plan.add("b", bld::Cmd{"true"});
            plan.produces("b", "demo_build/7e.o");
            auto res = bld::run(plan);
            expect_err("7d dup output", res, "multiple tasks produce");
        }
        // 7e. Unknown after-dependency.
        {
            bld::Plan plan;
            plan.add("a", bld::Cmd{"true"});
            plan.after("a", "ghost");
            auto res = bld::run(plan);
            expect_err("7e unknown dep", res, "unknown task 'ghost'");
        }
        // 7f. Self-dependency.
        {
            bld::Plan plan;
            plan.add("self", bld::Cmd{"true"});
            plan.after("self", "self");
            auto res = bld::run(plan);
            expect_err("7f self dep", res, "depends on itself");
        }
        // 7g. Cycle a<->b.
        {
            bld::Plan plan;
            plan.add("a", bld::Cmd{"true"});
            plan.add("b", bld::Cmd{"true"});
            plan.after("a", "b");
            plan.after("b", "a");
            auto res = bld::run(plan);
            expect_err("7g cycle", res, "cycle");
        }
        // 7h. Missing cwd.
        {
            auto res = bld::run(bld::Cmd{"true"}, bld::cwd{"demo_build/nope"});
            expect_proc_err("7h missing cwd", res, "does not exist");
        }
        // 7i. Eager open of a missing file (lazy form fails at spawn instead).
        {
            auto f = bld::io_in::open("demo_build/nope.txt");
            if (!f) {
                bld::log::i("7i eager open correctly failed: {}", f.error());
            } else {
                bld::log::e("7i eager open UNEXPECTED SUCCESS");
            }
            auto res = bld::run(bld::Cmd{"cat"}, bld::io_in{"demo_build/nope.txt"});
            expect_proc_err("7i lazy open at spawn", res, "nope.txt");
        }
        // 7j. Plan unknown after-dependency.
        {
            bld::Plan plan;
            plan.add("a", bld::Cmd{"true"});
            plan.after("a", "ghost");
            auto res = bld::run(plan);
            expect_err("7j plan unknown dep", res, "unknown task 'ghost'");
        }
    }

    // 8. Compile-time errors (uncomment to see the guided messages; none build).
    //    Each fires a static_assert naming the right modifier set.
    //   bld::run(tasks, bld::io_out{"x"});          // io routing is per-Task, not a Run modifier
    //   bld::run(cmd, bld::jobs{2});           // jobs is a Run modifier, not per-process
    //   bld::capture(cmd, bld::io_out{"x"});        // capture has no out routing (merged string)
    //   tasks.emplace_back(cmd, bld::jobs{2}); // Task takes Proc modifiers, not Run modifiers
    //   grp.run_new(cmd, bld::keep_going{});          // run_new takes Proc modifiers, not Run modifiers
    //   bld::run(tasks, bld::jobs{2}, bld::jobs{3}); // duplicate flag
    //   bld::run(cmd, bld::label{"a"}, bld::label{"b"});           // duplicate label
    //   bld::run(cmd, bld::cwd{"a"}, bld::cwd{"b"});               // duplicate cwd
    //   bld::run(cmd, bld::async{}, bld::async{});                 // duplicate async
    //   bld::run(cmd, bld::io_out{"a"}, bld::io_out{"b"});         // two out routes
    //   bld::run(cmd, bld::io_out{&s}, bld::io_out_err{"b"});         // io_out_err conflicts with io_out
    //   bld::run(cmd, bld::io_out_err{fd}, bld::io_err{"e"});         // io_out_err conflicts with io_err
    //   bld::run(cmd, bld::io_in{"a"}, bld::in_str{"b"});         // two stdin routes
    //   bld::wait_all(procs, bld::keep_going{});      // wait_all takes no modifiers

    return 0;
}
