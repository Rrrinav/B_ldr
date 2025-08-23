#include <unistd.h>
#include <chrono>
#include <string>
#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  // TODO: Make command-line parsing and config user friendly
  bld::Config::get().use_extra_config_keys = true;
  bld::Config::get().extra_config_val["test"] = "no";
  BLD_REBUILD_AND_ARGS();
  if (bld::Config::get().extra_config_val["test"] == "yes")
  {
    bld::Command c = { "g++", "-o", "./test", "./tests/core/main.cpp" };
    bld::execute(c);
    c.clear();
    bld::time::stamp start;
    c = { "./test" };
    bld::execute(c);
    auto e = bld::time::since<double, std::chrono::microseconds>(start);
    bld::log(bld::Log_type::INFO, "Time: " + std::to_string(e) + " microseconds" );
    return 0;
  }

  bld::time::stamp start;

  bld::Dep_graph graph{};

  graph.add_dep({"./main2",                                                // Target
                 {"./main2.cpp", "./foo.o", "./bar.o"},                    // Dependencies
                 {"g++", "main2.cpp", "-o", "main2", "foo.o", "bar.o"}});  // Command

  graph.add_dep({"./foo.o", {"./foo.cpp"}, {"g++", "-c", "foo.cpp", "-o", "foo.o"}});

  graph.add_dep({"./bar.o", {"./bar.cpp"}, {"g++", "-c", "bar.cpp", "-o", "bar.o"}});

  if (!graph.build_parallel("./main2", 3))
  {
    bld::log(bld::Log_type::ERR, "Build failed!");
    return 1;
  }
  auto e = bld::time::since<double, std::chrono::microseconds>(start);
  bld::log(bld::Log_type::INFO, "Time: " + std::to_string(e) + " microseconds");

  bld::log(bld::Log_type::INFO, "Build completed successfully!");
  return 0;
}
