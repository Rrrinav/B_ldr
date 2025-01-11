#define B_LDR_IMPLEMENTATION
#include "./b_ldr.hpp"

int main()
{
  b_ldr::Command cmd = {};

  cmd.parts = {"g++", "hello.cpp", "-o", "hello"};

  b_ldr::execute(cmd);

  return 0;
}
