#define B_LDR_IMPLEMENTATION
#include "./b_ldr.hpp"

int main()
{
  b_ldr::Command cmd = {};

  b_ldr::print_metadata();

  cmd.parts = {"g++", "hello.cpp", "-o", "hello"};

  if (b_ldr::execute(cmd) <= 0)
    return EXIT_FAILURE;

  b_ldr::execute_shell({"curl -s 'wttr.in/jammu?format=4'"});

  return 0;
}
