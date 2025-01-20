#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();

  // Handle command-line arguments
  BLD_HANDLE_ARGS();

  std::vector<bld::Command> cmds = {{"echo", "Hello world1"}, {"echo", "Hello world2"}, {"echo", "Hello world3"}, {"echo", "Hello world4"}};
  auto result = bld::execute_parallel(cmds, 5, true);
  if (result.completed < cmds.size())
    bld::log(bld::Log_type::WARNING, "At least one command failed to execute");
}
