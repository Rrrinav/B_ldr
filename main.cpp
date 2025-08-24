#include <unistd.h>
#include <chrono>
#include <string>
#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  BLD_REBUILD_YOURSELF_ONCHANGE();
  auto &config = bld::Config::get();
  config.add_flag("test", "Build and run tests instead of main project");
  BLD_HANDLE_ARGS();

  if (config["help"] || config["h"])
  {
    config.show_help();
    return 0;
  }

  if (config["test"])
  {
    bld::log(bld::Log_type::INFO, "Building and running tests...");

    std::string test_target = config["test"];
    bld::Command c = {"g++", "-o", "./test" , test_target + "/main.cpp"};

    if (!bld::execute(c))
    {
      bld::log(bld::Log_type::ERR, "Test build failed!");
      return 1;
    }

    bld::time::stamp start;
    c = {"./test"};
    if (!bld::execute(c))
    {
      bld::log(bld::Log_type::ERR, "Test execution failed!");
      return 1;
    }

    auto elapsed = bld::time::since<double, std::chrono::microseconds>(start);
    bld::log(bld::Log_type::INFO, "Test time: " + std::to_string(elapsed) + " microseconds");
    return 0;
  }

  bld::time::stamp start;
  bld::Dep_graph graph{};

  bld::Command main_cmd = {"g++", "main2.cpp", "-o", "main2", "foo.o", "bar.o"};
  bld::Command foo_cmd = {"g++", "-c", "foo.cpp", "-o", "foo.o"};
  bld::Command bar_cmd = {"g++", "-c", "bar.cpp", "-o", "bar.o"};

  // Add dependencies to graph
  graph.add_dep({"./" "main", {"./main2.cpp", "./foo.o", "./bar.o"}, main_cmd});

  graph.add_dep({"./foo.o", {"./foo.cpp"}, foo_cmd});
  graph.add_dep({"./bar.o", {"./bar.cpp"}, bar_cmd});

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
