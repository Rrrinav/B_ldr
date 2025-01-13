#define B_LDR_IMPLEMENTATION
#include "./b_ldr.hpp"

int main()
{
  b_ldr::Command cmd = {};
  b_ldr::print_metadata();

  cmd.parts = {"g++", "hello.cpp", "-o", "hello"};
  cmd.parts = {"ls", "-l"};

  b_ldr::execute_shell({"curl 'wttr.in/Jammu?format=4'"});

  if (b_ldr::execute(cmd) <= 0)
    return EXIT_FAILURE;

  return 0;
}
