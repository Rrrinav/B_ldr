#define B_LDR_IMPLEMENTATION
#include <cstdlib>  // For std::exit
#define USE_CONFIG

#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();
  // Handle command-line arguments
  HANDLE_ARGS();

  // Initialize configuration
  auto &config = bld::Config::get();

  if (config.compiler == "kk")
    std::cout << "Compiler!\n";

  config.save_to_file("build.conf");

  return 0;
}
