#include <unistd.h>
#include <chrono>
#include <string>
#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"

auto &cfg = bld::Config::get();

int main(int argc, char *argv[])
{
  BLD_REBUILD_YOURSELF_ONCHANGE();
  BLD_HANDLE_ARGS();

  bld::fs::walk_directory(".", [](bld::fs::Walk_fn_opt& opt) -> bool {
    if (bld::starts_with(opt.path.string(),"./.git")) opt.action = bld::fs::Walk_act::Ignore;
    if (opt.path.string() == "build.conf") opt.action = bld::fs::Walk_act::Stop;
    std::cout << opt.path.string() << std::endl;
    return true; // required
  });

  if (cfg["test"])
  {
    bld::log(bld::Log_type::INFO, "Building and running tests...");

    std::string test_target = cfg["test"];
    auto files = bld::fs::get_all_files_with_name(test_target, "main.cpp", true);

    std::string target = "test";

    auto run = [&] (std::string f) -> void {
      if (!bld::execute({cfg.compiler, "-o", target, f}))
        bld::log(bld::Log_type::ERR,"Execution failed!");
      if (!bld::execute({"./" + target}))
        bld::log(bld::Log_type::ERR,"Execution failed!");

      bld::fs::remove(target);
    };

    bld::time::stamp s;
    for (auto f: files )
    {
      s.reset();
      run(f);
      auto elapsed = bld::time::since<double, std::chrono::microseconds>(s);
      bld::log(bld::Log_type::INFO, "Test time: " + std::to_string(elapsed) + " microseconds");
    }
    return 0;
  }

  bld::Dep_graph graph{};

  bld::Command main_cmd = {"g++", "main2.cpp", "-o", "main2", "foo.o", "bar.o"};
  bld::Command foo_cmd  = {"g++", "-c", "foo.cpp", "-o", "foo.o"};
  bld::Command bar_cmd  = {"g++", "-c", "bar.cpp", "-o", "bar.o"};

  // Add dependencies to graph
  graph.add_dep({"./main", {"./main2.cpp", "./foo.o", "./bar.o"}, main_cmd});

  graph.add_dep({"./foo.o", {"./foo.cpp"}, foo_cmd});
  graph.add_dep({"./bar.o", {"./bar.cpp"}, bar_cmd});

  bld::time::stamp start;
  // Build with specified number of parallel jobs
  if (!graph.build_parallel("./main2"))
  {
    bld::log(bld::Log_type::ERR, "Build failed!");
    return 1;
  }

  auto elapsed = bld::time::since<double, std::chrono::microseconds>(start);
  bld::log(bld::Log_type::INFO, "Build time: " + std::to_string(elapsed) + " microseconds");
  bld::log(bld::Log_type::INFO, "Build completed successfully!");

  return 0;
}
