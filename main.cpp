#define B_LDR_IMPLEMENTATION
#include "./b_ldr.hpp"

int main()
{
  b_ldr::Command cmd = {};

  cmd.parts = {"g++", "hello.cpp", "-o", "hello"};
  cmd.parts = {"s", "-l"};
  if (b_ldr::execute(cmd) <= 0)
    return EXIT_FAILURE;

  return 0;
}
