#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"
#define BLD_USE_CONFIG

int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();

  // Handle command-line arguments
  BLD_HANDLE_ARGS();

  auto &config = bld::Config::get();

  std::vector<bld::Command> cmds = {{"echo", "Hello world1"},  //c
                                    {"echo", "Hello world2"},  //c
                                    {"echo", "Hello world3"},  //c
                                    {"echo", "Hello world4"}};

  auto result = bld::execute_parallel(cmds, config.threads, true);

  if (result.completed < cmds.size())
    bld::log(bld::Log_type::WARNING, "At least one command failed to execute");
}
