#define B_LDR_IMPLEMENTATION
#include <cstdlib>  // For std::exit
#define BLD_USE_CONFIG

#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();
  // Handle command-line arguments

  BLD_HANDLE_ARGS();

  // Initialize configuration
  auto &config = bld::Config::get();
  config.use_extra_config_keys = true;

  config.extra_config_bool["is_extra"] = false;

  // Provide compiler using command-line argument
  // main config -compiler=kk
  if (config.compiler == "kk")
    std::cout << "Compiler!\n";

  return 0;
}
