#define B_LDR_IMPLEMENTATION
#include "./b_ldr.hpp"

int main()
{
  Command cmd("ls", "-a");
  execute(cmd);
  return 0;
}
