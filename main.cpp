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
}
