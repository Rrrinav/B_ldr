// modules.cpp — scan C++20 modules and build them through Plan/run.
//
// g++ -std=c++23 -I. examples/modules.cpp -o /tmp/modules && /tmp/modules
//
// bld::fs::scan_modules finds `export module <name>;` declarations plus their
// `import` dependencies; here each module becomes a Plan task (stub `echo`
// commands so it runs on any compiler) wired with needs/produces/after, so
// the second run is up to date and skips everything.

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

int main()
{
    const std::string root = "demo_modules";
    std::ignore = bld::fs::make_dirs(root);
    if (!bld::fs::exists(root + "/a.cppm")) {
        std::ignore = bld::fs::write_file(root + "/a.cppm", "export module a;\n");
        std::ignore = bld::fs::write_file(root + "/b.cppm", "export module b;\nimport a;\n");
        std::ignore = bld::fs::write_file(root + "/main.cpp", "import b;\nint main(){return 0;}\n");
    }

    // 1. Scan.
    auto mods = bld::fs::scan_modules(root);
    bld::log::i("found {} module(s)", mods.size());
    for (const auto &m : mods) {
        std::string deps;
        for (const auto &d : m.imports) {
            deps += deps.empty() ? d : std::string{", "} + d;
        }
        bld::log::i("  module '{}' @ {} imports: {}", m.name, m.file.string(), deps.empty() ? "(none)" : deps);
    }

    auto stub = [](std::string_view out) {
#ifdef _WIN32
        return bld::Cmd{"cmd", "/c", std::format("echo built > {}", out)};
#else
        return bld::Cmd{"sh", "-c", std::format("echo built > {}", out)};
#endif
    };

    // 2. One task per module, wired to its sources and module dependencies.
    // Task names are module names, so `after` edges use the import list
    // filtered to modules present in this scan.
    std::unordered_set<std::string> known;
    for (const auto &m : mods) {
        known.insert(m.name);
    }
    auto out_for = [&](const std::string &mod_name) {
        std::string safe = mod_name;
        for (auto &c : safe) {
            if (c == ':') {
                c = '_';
            }
        }
        return root + "/built_" + safe + ".txt";
    };

    auto build_plan = [&]() {
        bld::Plan plan;
        for (const auto &m : mods) {
            std::string out = out_for(m.name);
            plan.add(m.name, stub(out));
            plan.needs(m.name, m.file.string());
            plan.produces(m.name, out);
            for (const auto &dep : m.imports) {
                if (known.contains(dep)) {
                    plan.after(m.name, dep);
                }
            }
        }
        return plan;
    };

    // 3. Build; a second run is up to date and skips everything.
    for (int round = 1; round <= 2; ++round) {
        bld::Plan plan = build_plan();
        auto res = bld::run(plan, bld::jobs{4});
        if (!res) {
            bld::log::e("plan build failed: {}", res.error());
            return EXIT_FAILURE;
        }
        bld::log::i("round {}: ran={} skipped={}", round, res->ran, res->skipped);
    }

    return EXIT_SUCCESS;
}
