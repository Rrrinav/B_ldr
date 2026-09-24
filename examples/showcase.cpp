// showcase.cpp — every b_ldr feature in one file
// Task carries name + Exec_spec, Plan owns the graph, Proc is a running Task
// g++ -std=c++23 examples/showcase.cpp -o /tmp/showcase && /tmp/showcase

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main(int argc, char *argv[]) {
    std::ignore = bld::rebuild_this_when_needed_ext(argc, argv);

    // 1. Logging + Env (portable names only, runtime thread-safe)
    bld::log::i("compiler={} cxx={} os={} arch={}",
                bld::env::compiler_name(), bld::env::cxx_standard_name(),
                bld::env::os_name(), bld::env::arch_name());
    std::ignore = bld::env::set("CC", "g++", true);
    bld::log::i("CC={}", bld::env::get_or("CC", "cc"));

    // 2. Cmd + Proc — Proc is running Task, Task is config for Proc (shared Exec_spec)
    //    Exec_spec { Cmd cmd; Proc_config cfg; } is the one member in both
    //    Task = name + spec (no gid), Proc = spec + P_id/Status + gid (only Proc has gid)
    {
        bld::Exec_spec spec;
        spec.cmd = bld::Cmd{"echo","hi"};
        spec.cfg.label = "echo-hi";
        spec.cfg.cwd = "demo_build";
        // Task is just name + spec, Proc is spec + P_id/Status/gid
        bld::Task t; t.name = "echo-hi"; t.spec = spec;
        auto proc = bld::Proc::spawn(t); // Task -> Proc
        if (proc) std::ignore = proc->wait();
        // also Proc::spawn(spec) directly
        auto proc2 = bld::Proc::spawn(spec);
        if (proc2) std::ignore = proc2->wait();
    }
    // 2b. Proc modifiers still work via Exec_spec.cfg (label/cwd/fd)
    {
        std::ignore = bld::run(bld::Cmd{"echo","hi"}, bld::label{"hi"}, bld::cwd{"demo_build"});
    }

    // 3. Proc_group — manages many Procs as one OS group (job object / pgid)
    {
        bld::Proc_group grp;
        auto id1 = grp.run_new(bld::Cmd{"sleep","0.1"}, bld::label{"a"});
        auto id2 = grp.run_new(bld::Cmd{"sleep","0.1"}, bld::label{"b"});
        if (id1 && id2) {
            auto done = grp.wait_any(); // blocks until any finishes, no polling
            if (done) std::ignore = grp.remove(*done);
            grp.signal(SIGTERM); // broadcast to all in group
        }
        // NOTE: don't grp.add() a Proc spawned without the group —
        // it has a different pgid and will panic (mismatched GID).
        // Always spawn via grp.run_new() so children join grp.gid().
    }

    // 4. Plan — dependency graph lives in Plan's side tables;
    //    Task itself carries no dependency info.
    {
        bld::Plan plan;
        // Task carries only name + spec (cmd+cfg). No inputs/outputs/after in Task.
        bld::Task a; a.name = "a.o"; a.spec.cmd = bld::Cmd{"sh","-c","echo a > demo_build/a.o"};
        bld::Task b; b.name = "b.o"; b.spec.cmd = bld::Cmd{"sh","-c","echo b > demo_build/b.o"};
        bld::Task app; app.name = "app"; app.spec.cmd = bld::Cmd{"sh","-c","cat demo_build/a.o demo_build/b.o > demo_build/app"};

        plan.add(std::move(a));
        plan.add(std::move(b));
        plan.add(std::move(app));

        // Plan manages what depends on what
        plan.produces("a.o", "demo_build/a.o");
        plan.needs("a.o", "examples/showcase.cpp");
        plan.produces("b.o", "demo_build/b.o");
        plan.needs("b.o", "examples/showcase.cpp");
        plan.produces("app", "demo_build/app");
        plan.needs("app", "demo_build/a.o");
        plan.needs("app", "demo_build/b.o");
        plan.after("app", "a.o"); // explicit edge
        plan.mark_compile_command("a.o");
        plan.mark_compile_command("b.o");

        // Every task gets a name: explicit -> first output -> cmd.str()
        bld::Task anon; anon.spec.cmd = bld::Cmd{"echo","anon"};
        plan.add(std::move(anon)); // name auto = "echo anon"

        auto res = bld::run(plan, bld::jobs{4}, bld::write_compile_commands{"demo_build/compile_commands.json"});
        if (!res) bld::log::e("plan failed: {}", res.error());
    }

    // 5. Unified run — one API for everything (parallel + async caps)
    {
        // simple
        std::ignore = bld::run(bld::Cmd{"echo","hi"});
        // via Plan (jobs width caps live procs, async cap too)
        bld::Plan p;
        bld::Task t; t.name = "x"; t.spec.cmd = bld::Cmd{"echo","x"};
        p.add(std::move(t));
        std::ignore = bld::run(p, bld::jobs{2}, bld::dry_run{});
        // via compile_commands.json
        if (bld::fs::exists("demo_build/compile_commands.json"))
            std::ignore = bld::run(bld::compile_commands("demo_build/compile_commands.json"), bld::jobs{4});
    }

    // 6. Filesystem, strings, time, config, is_outdated
    {
        std::ignore = bld::fs::make_dirs("demo_build");
        std::ignore = bld::fs::write_file("demo_build/hello.txt", "hi");
        auto txt = bld::fs::read_file("demo_build/hello.txt");
        bld::log::i("read: {}", txt.value_or("?"));
        bld::log::i("stem={} ext={}", bld::fs::stem("a/b.cpp"), bld::fs::extension("a/b.cpp"));
        bld::log::i("trim='{}'", bld::str::trim("  hi  "));
        auto t = bld::time::stamp{}; std::this_thread::sleep_for(std::chrono::milliseconds{5});
        bld::log::i("elapsed {}", bld::time::format(t.elapsed()));
        if (bld::is_outdated("demo_build/app", "examples/showcase.cpp"))
            bld::log::i("app outdated");
    }

    return 0;
}
