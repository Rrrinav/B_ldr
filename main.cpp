#define B_LDR_IMPLEMENTATION
#include <cstdlib>  // For std::exit

#include "./b_ldr.hpp"

int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();
  // Handle command-line arguments
  HANDLE_ARGS();

  // Initialize configuration
  auto &config = bld::Config::get();

  if (config.compiler == "clang++")
    std::cout << "Works!\n";

  if (config.hot_reload)
    std::cout << "Hot reload is enabled" << std::endl;

  config.save_to_file("build.conf");

  return 0;
  return 0;
}
