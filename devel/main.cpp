#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "../b_ldr.hpp"


struct Params
{
  bool reset = true;

  // Threaded execution
  std::vector<bld::Command> multi_thread_cmds = {};
  int max_threads      = (std::thread::hardware_concurrency() - 1) <= 0 ? std::thread::hardware_concurrency() : (std::thread::hardware_concurrency() - 1);
  bool parallel_strict = true;

  // Async process execution
  std::vector<bld::Proc> async_procs = {};
  int max_procs      = bld::get_n_procs();
  bool async_launch  = false;
  bool async_grouped = false;   // planned feature

  // I/O redirection
  bool use_redirect = false;
  int fd_in  = bld::INVALID_FD;
  int fd_out = bld::INVALID_FD;
  int fd_err = bld::INVALID_FD;

  // Fluent setters
  Params &with_reset(bool v)                                 { reset = v;              return *this; }
  Params &with_multi_thread_cmds(std::vector<bld::Command> v){ multi_thread_cmds = v;  return *this; }
  Params &with_max_threads(int m)                            { max_threads = m;        return *this; }
  Params &with_parallel_strict(bool v)                       { parallel_strict = v;    return *this; }
  Params &with_async_procs(std::vector<bld::Proc> ps)        { async_procs = ps;       return *this; }
  Params &with_async(bool v)                                 { async_launch = v;       return *this; }
  Params &with_max_procs(bool v)                             { max_procs = v;          return *this; }
  Params &with_async_grouped(bool v)                         { async_grouped = v;      return *this; }

  Params &with_redirect(bool v)                              { use_redirect = v;       return *this; }
  Params &stdin_fd(int fd)                                   { fd_in = fd;             return *this; }
  Params &stdout_fd(int fd)                                  { fd_out = fd;            return *this; }
  Params &stderr_fd(int fd)                                  { fd_err = fd;            return *this; }
};


int exec_opts(bld::Command &cmd, const Params &opts)
{
  bld::Command new_cmd = cmd;
  if (opts.reset)
    cmd.clear();

  if (!opts.multi_thread_cmds.empty())
    return bld::execute_threads(opts.multi_thread_cmds, opts.max_threads, opts.parallel_strict).completed - (int)opts.multi_thread_cmds.size() + 1;

  if (opts.async_launch || (!opts.async_procs.empty()))
  {
    if (opts.async_procs.empty())
    {
      if (opts.use_redirect)
        return bld::execute_async_redirect(new_cmd, bld::Redirect(opts.fd_in, opts.fd_out, opts.fd_err)).p_id;
      else
        return bld::execute_async(new_cmd).p_id;
    }

    if (opts.async_grouped)
    {
      bld::log(bld::Log_type::ERR, "Async groups not implemented yet!");
      std::abort();
    }
    else
    {
      return bld::wait_procs(opts.async_procs).completed - (int)opts.async_procs.size() + 1;
    }
  }

  if (opts.use_redirect)
    return bld::execute_redirect(new_cmd, bld::Redirect(opts.fd_in, opts.fd_out, opts.fd_err));

  return bld::execute(new_cmd);
}

#define run_cmd(cmd, ...) execute_with_opts(cmd, Params{} __VA_ARGS__)

int main(int argc, char *argv[])
{
  BLD_REBUILD_AND_ARGS();
  bld::time::stamp start;
  sleep(1);
  bld::log(bld::Log_type::INFO, "Time: " + std::to_string(bld::time::time_since<double, std::chrono::seconds>(start)));
  bld::Command cmd = {"echo", "'Hello'"};
  return 0;

  bld::Dep_graph graph{};

  graph.add_dep({"./main2",                                                // Target
                 {"./main2.cpp", "./foo.o", "./bar.o"},                    // Dependencies
                 {"g++", "main2.cpp", "-o", "main2", "foo.o", "bar.o"}});  // Command

  graph.add_dep({"./foo.o", {"./foo.cpp"}, {"g++", "-c", "foo.cpp", "-o", "foo.o"}});

  graph.add_dep({"./bar.o", {"./bar.cpp"}, {"g++", "-c", "bar.cpp", "-o", "bar.o"}});

  if (!graph.build_parallel("./main2", 3))
  {
    bld::log(bld::Log_type::ERR, "Build failed!");
    return 1;
  }

  bld::log(bld::Log_type::INFO, "Build completed successfully!");
  return 0;
  
}
