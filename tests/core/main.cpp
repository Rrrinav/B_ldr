#include <array>
#include <ostream>
#include <string>
#define B_LDR_IMPLEMENTATION
#include "../../b_ldr.hpp"

struct Test
{
  int pass{};
  int id{};
  std::string name = "";

  void print()
  {
    bld::log(bld::Log_type::INFO, name);
    std::cout << "    [ " + std::to_string(id) + " ]: " + (pass == 0 ? "failed" : "passed") << std::endl;
  }
};

std::string code = R"(
  #include <iostream>
  int main() {
    std::cout << "Test";
    return 0;
  }
)";

std::string code_signal = R"(
  #include <iostream>
  #include <thread>
  #include <chrono>
  int main() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 0;
  }
)";

std::string code_sleep = R"(
  #include <iostream>
  #include <thread>
  #include <chrono>
  int main() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
  }
)";

std::string code_io = R"(
#include <iostream>
#include <string>

int main() {
  std::string line;
  if (std::getline(std::cin, line)) {
      std::cout << line;
  } else {
      std::cerr << "No input received on stdin." << std::endl;
  }
  std::cerr << "Test err";
  return 0;
})";

const int TOTAL_TESTS = 12;
int TEST_FAILED = 0;
std::array<Test, TOTAL_TESTS> tests{};
int id = 1;
bld::Command cmd = {"g++", "-o", "test", "./test1.cpp"};

void test_execute()
{
  tests[0] = {0, id++, "Basic execute no file."};
  auto e = bld::execute(cmd);

  if (e.exit_code == 1)
    tests[0].pass = 1;
  else
    TEST_FAILED++;

  bld::fs::write_entire_file("./test1.cpp", code);

  tests[1] = {0, id++, "Basic execute."};

  e = bld::execute(cmd);

  if (e.exit_code == 0 && e.normal)
    tests[1].pass = 1;
  else
    TEST_FAILED++;

  bld::fs::remove("./test1.cpp", "test");
}

void test_async()
{
  bld::fs::write_entire_file("./test1.cpp", code_signal);

  tests[2] = {0, id++, "Signal execute async and wait_proc."};

  auto p = bld::execute_async(cmd);
  kill(p.p_id, 6);

  auto e = bld::wait_proc(p);

  if (e.signal == 6 && !e.normal)
    tests[2].pass = 1;
  else
    TEST_FAILED++;

  bld::fs::remove("./test1.cpp", "test");
}

void test_redirect()
{
  tests[3] = {0, id++, "execute redirect single."};
  bld::fs::write_entire_file("./test1.cpp", code);
  if (bld::execute(cmd))
  {
    bld::fs::write_entire_file("./output", "");
    auto fd = bld::open_for_write("./output");
    bld::execute_redirect({"./test"}, bld::Redirect(bld::INVALID_FD, fd, bld::INVALID_FD));
    bld::close_fd(fd);
    std::string s = "";
    if (bld::fs::read_file("./output", s))
    {
      if ("Test" == s)
        tests[3].pass = 1;
      else
        TEST_FAILED++;
    }
    bld::fs::remove("./output", "./test1", "test");
  }

  tests[4] = {0, id++, "execute async redirect multiple."};

  bld::fs::write_entire_file("./test1.cpp", code_io);
  if (bld::execute(cmd))
  {
    bld::fs::write_entire_file("./output", "");
    bld::fs::write_entire_file("./error", "");
    auto s = "Heya testing";
    bld::fs::write_entire_file("./input", "Heya testing");
    auto fd_out = bld::open_for_write("./output");
    auto fd_in = bld::open_for_read("./input");
    auto fd_er = bld::open_for_write("./error");
    bld::execute_redirect({"./test"}, bld::Redirect(fd_in, fd_out, fd_er));
    bld::close_fd(fd_out, fd_in, fd_er);
    std::string o = "", i = "", e = "";
    if (bld::fs::read_file("./output", o) && bld::fs::read_file("./input", i) && bld::fs::read_file("./error", e))
    {
      if (o == s && i == s && e == "Test err")
        tests[4].pass = 1;
      else
        TEST_FAILED++;
    }
    bld::fs::remove("./output", "./error", "./input", "./test1", "test");
  }
}

void test_wait_and_cleanup()
{
  bld::fs::write_entire_file("./test1.cpp", code_signal);
  tests[5] = {0, id++, "wait_proc + cleanup_process + try_wait_nb"};

  auto proc = bld::execute_async(cmd);

  // non-blocking wait: should be running at first
  auto st = bld::try_wait_nb(proc);
  if (!st.exited && !st.invalid_proc)
  {
    // now wait for real
    auto e = bld::wait_proc(proc);
    if (e.exit_code == 0 && e.normal)
    {
      // cleanup
      bld::cleanup_process(proc);
      if (proc.p_id == -1)
      {
        tests[5].pass = 1;
        return;
      }
    }
  }
  TEST_FAILED++;
  bld::fs::remove("./test1.cpp", "test");
}

void test_wait_procs()
{
  bld::fs::write_entire_file("./test1.cpp", code_sleep);

  tests[6] = {0, id++, "wait_procs multiple processes"};

  std::vector<bld::Proc> procs;
  for (int i = 0; i < 3; i++) procs.push_back(bld::execute_async(cmd));

  auto res = bld::wait_procs(procs, 20);

  if (res.completed == 3 && res.failed_indices.empty())
    tests[6].pass = 1;
  else
    TEST_FAILED++;

  bld::fs::remove("./test1.cpp", "test");
}

void test_async_redirect()
{
  tests[7] = {0, id++, "execute_async_redirect"};

  bld::fs::write_entire_file("./test1.cpp", code_io);
  if (bld::execute(cmd))
  {
    bld::fs::write_entire_file("./input", "Hello");
    auto fd_in = bld::open_for_read("./input");
    auto fd_out = bld::open_for_write("./output");
    auto fd_err = bld::open_for_write("./error");

    auto proc = bld::execute_async_redirect({"./test"}, {fd_in, fd_out, fd_err});
    auto st = bld::wait_proc(proc);

    bld::close_fd(fd_in, fd_out, fd_err);

    std::string o, e;
    bld::fs::read_file("./output", o);
    bld::fs::read_file("./error", e);

    if (st && o == "Hello" && e == "Test err")
      tests[7].pass = 1;
    else
      TEST_FAILED++;
    bld::fs::remove("./input", "./output", "./error", "./test1.cpp", "test");
  }
}

void test_execute_threads()
{
  tests[8] = {0, id++, "execute_threads"};

  bld::fs::write_entire_file("./test1.cpp", code);

  std::vector<bld::Command> cmds(4, cmd);
  auto res = bld::execute_threads(cmds, 2);

  if (res.completed == cmds.size() && res.failed_indices.empty())
    tests[8].pass = 1;
  else
    TEST_FAILED++;

  bld::fs::remove("./test1.cpp", "test");
}

void test_shell()
{
  tests[9] = {0, id++, "execute_shell"};
  int res = bld::execute_shell("echo HelloShell > shellout.txt");
  std::string s;
  if (res == 0 && bld::fs::read_file("shellout.txt", s) && s.find("HelloShell") != std::string::npos)
    tests[9].pass = 1;
  else
    TEST_FAILED++;
  bld::fs::remove("shellout.txt");
}

void test_read_output()
{
  tests[10] = {0, id++, "read_process_output"};
  bld::fs::write_entire_file("./test1.cpp", code);
  if (bld::execute(cmd))
  {
    std::string out;
    if (bld::read_process_output({"./test"}, out) && out == "Test")
      tests[10].pass = 1;
    else
      TEST_FAILED++;
  }
  bld::fs::remove("./test1.cpp", "test");

  tests[11] = {0, id++, "read_shell_output"};
  std::string out;
  if (bld::read_shell_output("echo HelloWorld", out) && out.find("HelloWorld") != std::string::npos)
    tests[11].pass = 1;
  else
    TEST_FAILED++;
}

int main(int argc, char *argv[])
{
  BLD_REBUILD_YOURSELF_ONCHANGE();
  test_execute();
  test_async();
  test_redirect();
  test_wait_and_cleanup();
  test_wait_procs();
  test_async_redirect();
  test_execute_threads();
  test_shell();
  test_read_output();

  bld::fs::remove("./test1.cpp", "test");

  std::cout << std::endl << std::endl;
  int passed = TOTAL_TESTS - TEST_FAILED;
  std::cout << "----------------------------------------------------------" << std::endl;
  bld::log(bld::Log_type::INFO, "Total tests:  " + std::to_string(TOTAL_TESTS));
  bld::log(bld::Log_type::INFO, "Tests passed: " + std::to_string(passed));
  bld::log(bld::Log_type::INFO, "Tests failed: " + std::to_string(TEST_FAILED));
  std::cout << "----------------------------------------------------------\n" << std::endl;
  for (auto t : tests) t.print();

  return 0;
}
