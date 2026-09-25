// groups.cpp — tasks that hold tasks.
//
// g++ -std=c++23 -I. examples/groups.cpp -o /tmp/groups && /tmp/groups
//
// A group task spawns nothing itself; its subtasks run as dotted-name
// leaves ("build.compile") sharing the group's graph edges. A bare span
// runs every leaf in parallel; in a Plan the group is a single node.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    // 1. Span: a group is just more parallel leaves.
    {
        bld::Task build;
        build.name = "build";
        bld::Task compile{bld::Cmd{"sh", "-c", "echo compiling"}};
        compile.name = "compile";
        bld::Task link{bld::Cmd{"sh", "-c", "echo linking"}};
        link.name = "link";
        build.sub(std::move(compile)).sub(std::move(link));

        std::vector<bld::Task> tasks;
        tasks.push_back(std::move(build));
        tasks.emplace_back(bld::Cmd{"sh", "-c", "echo testing"});
        if (auto res = bld::run(tasks, bld::jobs{4}); res) {
            bld::log::i("1. span with group ran={} (2 leaves + 1 task)", res->ran);
        }
    }

    // 2. Nesting dots the names: outer.inner.x.
    {
        bld::Task inner;
        inner.name = "inner";
        bld::Task leaf{bld::Cmd{"sh", "-c", "echo deep"}};
        leaf.name = "x";
        inner.sub(std::move(leaf));
        bld::Task outer;
        outer.name = "outer";
        outer.sub(std::move(inner));

        std::vector<bld::Task> tasks;
        tasks.push_back(std::move(outer));
        if (auto res = bld::run(tasks, bld::jobs{2}); res) {
            bld::log::i("2. nested group ran={} (leaf outer.inner.x)", res->ran);
        }
    }

    // 3. Plan: the group is one dependency node; "test" waits for all leaves.
    {
        std::ignore = bld::fs::make_dirs("demo_groups");
        bld::Plan plan;
        bld::Task build;
        build.name = "build";
        bld::Task c1{bld::Cmd{"sh", "-c", "echo one > demo_groups/1.o"}};
        c1.name = "c1";
        bld::Task c2{bld::Cmd{"sh", "-c", "echo two > demo_groups/2.o"}};
        c2.name = "c2";
        build.sub(std::move(c1)).sub(std::move(c2));
        plan.add(std::move(build));
        plan.add("test", bld::Cmd{"sh", "-c", "cat demo_groups/1.o demo_groups/2.o > demo_groups/out.o"});
        plan.after("test", "build");
        if (auto res = bld::run(plan, bld::jobs{2}); res) {
            bld::log::i("3. plan with group ran={}", res->ran);
        }
        if (auto out = bld::fs::read_file("demo_groups/out.o")) {
            bld::log::i("3. test ran after the whole group, out='{}'", bld::str::trim(*out));
        }
        std::filesystem::remove_all("demo_groups");
    }

    // 4. Misuse fails fast with a name attached.
    {
        bld::Task g{bld::Cmd{"true"}};
        g.name = "g";
        g.sub(bld::Task{bld::Cmd{"true"}});
        std::vector<bld::Task> tasks;
        tasks.push_back(std::move(g));
        if (auto res = bld::run(tasks); !res) {
            bld::log::i("4. command+subtasks correctly failed: {}", res.error());
        }
    }

    return EXIT_SUCCESS;
}
