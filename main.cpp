#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  BLD_REBUILD_AND_ARGS();

  bld::Dep_graph graph{};

  graph.add_dep({"./main2",                                              // Target
                {"./main2.cpp", "./foo.o", "./bar.o"},                   // Dependencies
                {"g++", "main2.cpp", "-o", "main2", "foo.o", "bar.o"}}); // Command

  graph.add_dep({"./foo.o", {"./foo.cpp"}, {"g++", "-c", "foo.cpp", "-o", "foo.o"}});

  graph.add_dep({"./bar.o", {"./bar.cpp"}, {"g++", "-c", "bar.cpp", "-o", "bar.o"}});

  if (!graph.build_parallel("./main2", 3))
  {
    bld::log(bld::Log_type::ERR, "Build failed!");
    return 1;
  }

  bld::log(bld::Log_type::INFO, "Build completed successfully!");
  return 0;
}
