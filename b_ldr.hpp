/*
Copyright Dec 2025, Rinav (github: rrrinav)

  Permission is hereby granted, free of charge,
  to any person obtaining a copy of this software and associated documentation files(the “Software”),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
  sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions :

  The above copyright notice and this permission notice shall be included in all copies
  or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”,
  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <cerrno>
#include <clocale>
#include <cstring>
#include <string>
#include <vector>

/* @brief: Rebuild the build executable if the source file is newer than the executable and run it
 * @description: Takes no parameters and calls bld::rebuild_yourself_onchange_and_run() with the current file and executable.
 *    bld::rebuild_yourself_onchange() detects compiler itself (g++, clang++, cl supported) and rebuilds the executable
 *    if the source file is newer than the executable. It then restarts the new executable and exits the current process.
 * If you want more cntrol, use bld::rebuild_yourself_onchange() or bld::rebuild_yourself_onchange_and_run() directly.
 */
#define BLD_REBUILD_YOURSELF_ONCHANGE() bld::rebuild_yourself_onchange_and_run(__FILE__, argv[0])
// Same but with compiler specified...
#define C_BLD_REBUILD_YOURSELF_ONCHANGE(compiler) bld::rebuild_yourself_onchange_and_run(__FILE__, argv[0], compiler)

#define HANDLE_ARGS() bld::handle_args(argc, argv)

namespace bld
{
  // Log type is enumeration for bld::function to show type of log
  enum class Log_type
  {
    INFO,
    WARNING,
    ERROR,
    DEBUG
  };

  /* @param type ( Log_type enum ): Type of log
   * @param msg ( std::string ): Message to log
   * @description: Function to log messages with type
   */
  void log(Log_type type, const std::string &msg);

  // Struct to hold command parts
  struct Command
  {
    std::vector<std::string> parts;  // > parts of the command

    // Default constructor
    Command() : parts{} {}

    // @tparam args ( variadic template ): Command parts
    template <typename... Args>
    Command(Args... args);

    // @tparam args ( variadic template ): Command parts
    template <typename... Args>
    void add_parts(Args... args);

    /* @return ( std::string ): Get the full command as a single string
     * @description: Get the full command as a single string
     */
    std::string get_command_string() const;

    /* @return ( std::vector<char *> ): Get the full command but as C-style arguments for `execvp`
     * @description: Get the full command as a C-style arguments for sys calls
     */
    std::vector<char *> to_exec_args() const;

    // @return ( bool ): Check if the command is empty
    bool is_empty() const { return parts.empty(); }

    // @return ( std::string ): Get the command as a printable string wrapped in single quotes
    std::string get_print_string() const;
  };

  class Config
  {
  private:
    Config();
    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;

  public:
    bool hot_reload;
    bool verbose;
    bool override_run;
    bool extra_args;
    std::string compiler;
    std::string target_executable;
    std::string target_platform;
    std::string build_dir;
    std::string compiler_flags;
    std::string linker_flags;
    std::string pre_build_command;
    std::string post_build_command;
    std::string default_run_args;
    std::vector<std::string> hot_reload_files;
    std::vector<std::string> cmd_args;

    static Config &get();

    void init();

    bool load_from_file(const std::string &filename);

    bool save_to_file(const std::string &filename);
  };

  /* @brief: Convert command-line arguments to vector of strings
   * @param argc ( int ): Number of arguments
   * @param argv ( char*[] ): Array of arguments
   */
  bool args_to_vec(int argc, char *argv[], std::vector<std::string> &args);

  /* @beief: Validate the command before executing
   * @param command ( Command ): Command to validate
   */
  bool validate_command(const Command &command);

  /* @brief: Wait for the process to complete
   * @param pid ( pid_t ): Process ID to wait for
   * @description: Wait for the process to complete and log the status. Use this function instead of direct waitpid
   */
  int wait_for_process(pid_t pid);

  /* @brief: Execute the command
   * @param command ( Command ): Command to execute, must be a valid process command and not shell command
   * @return: returns a code to indicate success or failure
   *   >0 : Command executed successfully, returns pid of fork.
   *    0 : Command failed to execute or something wrong on system side
   *   -1 : No command to execute or something wrong on user side
   * @description: Execute the command using fork and log the status alongwith
   */
  int execute(const Command &command);

  /* @description: Print system metadata:
   *  1. Operating System
   *  2. Compiler
   *  3. Architecture
   */
  void print_metadata();

  /* @brief: Preprocess the command for shell execution
   * @param cmd ( Command ): Command to preprocess
   * @return: Preprocessed command for shell execution
   * @description: Preprocess the command for shell execution by adding shell command and arguments
   *    Windows:     cmd.exe /c <command>
   *    Linux/macOS: /bin/sh -c <command>
   */
  Command preprocess_commands_for_shell(const Command &cmd);

  /* @brief: Execute the shell command with preprocessed parts
   * param cmd ( Command ): Command to execute in shell
   * @description: Execute the shell command with preprocessed parts
   *    Uses execute function to execute the preprocessed command
   *  return the return value of the execute function
   */
  int execute_shell(std::string command);

  /* @brief: Execute the shell command with preprocessed parts but asks wether to execute or not first
   * @param cmd ( Command ): Command to execute in shell
   * @param prompt ( bool ): Ask for confirmation before executing
   * @description: Execute the shell command with preprocessed parts with prompting
   *    Uses execute function to execute the preprocessed command
   *    return the return value of the execute function
   */
  int execute_shell(std::string command, bool prompt);

  /* @brief: Read output from a process command execution
   * @param cmd ( Command ): Command struct containing the process command and arguments
   * @param output ( std::string& ): Reference to string where output will be stored
   * @param buffer_size ( size_t ): Size of buffer for reading output (default: 4096)
   * @return ( bool ): true if successful, false otherwise
   */
  bool read_process_output(const Command &cmd, std::string &output, size_t buffer_size = 4096);

  /* @brief: Read output from a shell command execution
   * @param shell_cmd ( std::string ): Shell command to execute and read output from
   * @param output ( std::string& ): Reference to string where output will be stored
   * @param buffer_size ( size_t ): Size of buffer for reading output (default: 4096)
   * @return ( bool ): true if successful, false otherwise
   */
  bool read_shell_output(const std::string &shell_cmd, std::string &output, size_t buffer_size = 4096);

  /* @brief: Check if the executable is outdated i.e. source file is newer than the executable
   * @param file_name ( std::string ): Source file name
   * @param executable ( std::string ): Executable file name
   * @description: Check if the source file is newer than the executable. Uses last write time to compare.
   *  Can be used for anything realistically, just enter paths and it will return.
   *  it basically returns ( modify_time(file) > modify_time(executable) ) irrespective of file type
   */
  bool is_executable_outdated(std::string file_name, std::string executable);

  /* @brief: Rebuild the executable if the source file is newer than the executable and runs it
   * @param filename ( std::string ): Source file name
   * @param executable ( std::string ): Executable file name
   * @param compiler ( std::string ): Compiler command to use (default: "")
   *  It can detect compiler itself if not provided
   *  Supported compilers: g++, clang++, cl
   *  @description: Generally used for actual build script
   */
  void rebuild_yourself_onchange_and_run(const std::string &filename, const std::string &executable, std::string compiler = "");

  /* @brief: Rebuild the executable if the source file is newer than the executable
   * @param filename ( std::string ): Source file name (C++ only)
   * @param executable ( std::string ): Executable file name
   * @param compiler ( std::string ): Compiler command to use (default: "")
   *  It can detect compiler itself if not provided
   *  Supported compilers: g++, clang++, cl
   *  @description: Actually for general use and can be used to rebuild any *C++* file since it doesn't restart the process
   */
  void rebuild_yourself_onchange(const std::string &filename, const std::string &executable, std::string compiler);

  int handle_run_command(std::vector<std::string> args)
  {
    if (args.size() > 1)
    {
      bld::log(bld::Log_type::ERROR, "Too many args for 'run' command");
      return -1;
    }
    if (args.size() == 2)
    {
      bld::log(bld::Log_type::WARNING, "Command 'run' specified with the command");
      bld::log(bld::Log_type::INFO, "Proceeding to run the specified command: " + args[1]);
      bld::Command cmd(args[1]);
      return bld::execute(cmd);
    }

    if (Config::get().target_executable.empty())
    {
      bld::log(bld::Log_type::ERROR, "No target executable specified in config");
      return -1;
    }

    bld::Command cmd;
    cmd.parts.push_back(Config::get().target_executable);

    return bld::execute(cmd);
  }

  bool starts_with(const std::string &str, const std::string &prefix)
  {
    if (prefix.size() > str.size())
      return false;
    return str.compare(0, prefix.size(), prefix) == 0;
  }

  void handle_config_command(std::vector<std::string> args, std::string name)
  {
    if (args.size() < 2)
    {
      log(Log_type::ERROR, "Config command requires arguments");
      std::string usage = name + " config -[key]=value \n" + "        E.g: ' " + name + " config -verbose=true '";
      log(Log_type::INFO, "Usage: " + usage);
      return;
    }

    auto &config = Config::get();

    for (size_t i = 1; i < args.size(); i++)
    {
      const auto &arg = args[i];
      if (starts_with(arg, "-hreload="))
        config.hot_reload = (arg.substr(9) == "true");
      else if (starts_with(arg, "-hreload"))
        config.hot_reload = true;
      else if (starts_with(arg, "-compiler="))
        config.compiler = arg.substr(10);
      else if (starts_with(arg, "-target="))
        config.target_executable = arg.substr(8);
      else if (starts_with(arg, "-build_dir="))
        config.build_dir = arg.substr(11);
      else if (starts_with(arg, "-compiler_flags="))
        config.compiler_flags = arg.substr(16);
      else if (starts_with(arg, "-linker_flags="))
        config.linker_flags = arg.substr(14);
      else if (starts_with(arg, "-verbose="))
        config.verbose = (arg.substr(9) == "true");
      else if (starts_with(arg, "-v"))
        config.verbose = true;
      else if (starts_with(arg, "-pre_build_command="))
        config.pre_build_command = arg.substr(19);
      else if (starts_with(arg, "-post_build_command="))
        config.post_build_command = arg.substr(20);
      else if (starts_with(arg, "-default_run_args="))
        config.default_run_args = arg.substr(18);
      else if (starts_with(arg, "-override_run="))
        config.override_run = (arg.substr(14) == "true");
      else if (!config.extra_args)
        bld::log(bld::Log_type::ERROR, "Unknown argument: " + arg);
    }

    // Save the updated configuration to file
    config.save_to_file("build.conf");
    bld::log(bld::Log_type::INFO, "Configuration saved to build.conf");
  }

  void handle_args(int argc, char *argv[])
  {
    std::vector<std::string> args;
    if (args_to_vec(argc, argv, args))
    {
      Config::get().cmd_args = args;
      if (args.size() <= 0)
        return;
      else
      {
        std::string command = args[0];
        if (command == "run" && !Config::get().override_run)
          handle_run_command(args);
        else if (command == "config")
          handle_config_command(args, argv[0]);
      }
    }
  }
}  // namespace bld

//#define B_LDR_IMPLEMENTATION
#ifdef B_LDR_IMPLEMENTATION

#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>

void bld::log(bld::Log_type type, const std::string &msg)
{
  switch (type)
  {
    case Log_type::INFO:
      std::cout << "[INFO]: " << msg << std::endl;
      break;
    case Log_type::WARNING:
      std::cout << "[WARNING]: " << msg << std::endl;
      break;
    case Log_type::ERROR:
      std::cerr << "[ERROR]: " << msg << std::flush << std::endl;
      break;
    case Log_type::DEBUG:
      std::cout << "[DEBUG]: " << msg << std::flush << std::endl;
      break;
    default:
      std::cout << "[UNKNOWN]: " << msg << std::endl;
      break;
  }
}

// Get the full command as a single string
std::string bld::Command::get_command_string() const
{
  std::stringstream ss;
  for (const auto &part : parts)
    ss << part << " ";
  return ss.str();
}

// Convert parts to C-style arguments for `execvp`
std::vector<char *> bld::Command::to_exec_args() const
{
  std::vector<char *> exec_args;
  for (const auto &part : parts)
    exec_args.push_back(const_cast<char *>(part.c_str()));
  exec_args.push_back(nullptr);  // Null terminator
  return exec_args;
}

std::string bld::Command::get_print_string() const
{
  std::stringstream ss;
  ss << "' " << parts[0];
  if (parts.size() == 1)
    return ss.str() + "'";

  for (int i = 1; i < parts.size(); i++)
    ss << " " << parts[i];

  ss << " '";

  return ss.str();
}

template <typename... Args>
bld::Command::Command(Args... args)
{
  (parts.emplace_back(args), ...);
}

template <typename... Args>
void bld::Command::add_parts(Args... args)
{
  (parts.emplace_back(args), ...);
}

bool bld::validate_command(const bld::Command &command)
{
  bld::log(bld::Log_type::WARNING, "Do you want to execute " + command.get_print_string() + "in shell");
  std::cerr << "  [WARNING]: Answer[y/n]: ";
  std::string response;
  std::getline(std::cin, response);
  return (response == "y" || response == "Y");
}

int bld::wait_for_process(pid_t pid)
{
  int status;
  waitpid(pid, &status, 0);  // Wait for the process to complete

  if (WIFEXITED(status))
  {
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0)
    {
      bld::log(bld::Log_type::ERROR, "Process exited with non-zero status: " + std::to_string(exit_code));
      return exit_code;  // Return exit code for failure
    }
    bld::log(bld::Log_type::INFO, "Process exited successfully.");
  }
  else if (WIFSIGNALED(status))
  {
    int signal = WTERMSIG(status);
    bld::log(bld::Log_type::ERROR, "Process terminated by signal: " + std::to_string(signal));
    return -1;  // Indicate signal termination
  }
  else
  {
    bld::log(bld::Log_type::WARNING, "Unexpected process termination status.");
  }

  return static_cast<int>(pid);  // Indicate success
}

int bld::execute(const Command &command)
{
  if (command.is_empty())
  {
    bld::log(Log_type::ERROR, "No command to execute.");
    return -1;
  }

  auto args = command.to_exec_args();
  bld::log(Log_type::INFO, "Executing command: " + command.get_print_string());

  pid_t pid = fork();
  if (pid == -1)
  {
    bld::log(Log_type::ERROR, "Failed to create child process.");
    return 0;
  }
  else if (pid == 0)
  {
    // Child process
    if (execvp(args[0], args.data()) == -1)
    {
      perror("execvp");
      bld::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
      exit(EXIT_FAILURE);
    }
    // This line should never be reached
    bld::log(Log_type::ERROR, "Unexpected code execution after execvp. Did we find a bug? in libc or kernel?");
    abort();
  }

  return bld::wait_for_process(pid);  // Use wait_for_process instead of direct waitpid
}

void bld::print_metadata()
{
  std::cout << '\n';
  log(Log_type::INFO, "Printing system metadata...........................................");
  // 1. Get OS information
  struct utsname sys_info;
  if (uname(&sys_info) == 0)
  {
    std::cout << "    Operating System: " << sys_info.sysname << " " << sys_info.release << " (" << sys_info.machine << ")" << std::endl;
  }
  else
  {
#ifdef _WIN32
    std::cout << "    Operating System: Windows" << std::endl;
#else
    std::cerr << "Failed to get OS information." << std::endl;
#endif
  }

  // 2. Compiler information
  std::cout << "    Compiler:         ";
#ifdef __clang__
  std::cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
#elif defined(__GNUC__)
  std::cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#elif defined(_MSC_VER)
  std::cout << "MSVC " << _MSC_VER;
#else
  std::cout << "Unknown Compiler";
#endif
  std::cout << std::endl;

  // 3. Architecture
#if defined(__x86_64__) || defined(_M_X64)
  std::cout << "    Architecture:     64-bit" << std::endl;
#elif defined(__i386) || defined(_M_IX86)
  std::cout << "    Architecture:     32-bit" << std::endl;
#else
  std::cout << "    Architecture:     Unknown" << std::endl;
#endif
  log(Log_type::INFO, "...................................................................\n");
}

bld::Command bld::preprocess_commands_for_shell(const bld::Command &cmd)
{
  bld::Command cmd_s;

// Detect OS and adjust the shell command
#ifdef _WIN32
  // Windows uses cmd.exe
  cmd_s.add_parts("cmd", "/c", cmd.get_command_string());
#else
  // Unix-based systems (Linux/macOS)
  cmd_s.add_parts("/bin/sh", "-c", cmd.get_command_string());
#endif

  return cmd_s;
}

// Execute the shell command with preprocessed parts
int bld::execute_shell(std::string cmd)
{
  // Preprocess the command for shell execution
  auto cmd_s = preprocess_commands_for_shell(cmd);

  // Execute the shell command using the original execute function
  return execute(cmd_s);
}

int bld::execute_shell(std::string cmd, bool prompt)
{
  if (prompt)
  {
    if (validate_command(cmd))
    {
      // Preprocess the command for shell execution
      auto cmd_s = preprocess_commands_for_shell(cmd);

      // Execute the shell command using the original execute function
      return execute(cmd_s);
    }
    else
    {
      return -1;
    }
  }
  auto cmd_s = preprocess_commands_for_shell(cmd);

  // Execute the shell command using the original execute function
  return execute(cmd_s);
}

bool bld::read_process_output(const Command &cmd, std::string &output, size_t buffer_size)
{
  if (cmd.is_empty())
  {
    bld::log(Log_type::ERROR, "No command to execute.");
    return false;
  }

  if (buffer_size == 0)
  {
    bld::log(Log_type::ERROR, "Buffer size cannot be zero.");
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    bld::log(Log_type::ERROR, "Failed to create pipe: " + std::string(strerror(errno)));
    return false;
  }

  auto args = cmd.to_exec_args();
  bld::log(Log_type::INFO, "Extracting output from: " + cmd.get_print_string());

  pid_t pid = fork();
  if (pid == -1)
  {
    bld::log(Log_type::ERROR, "Failed to create child process: " + std::string(strerror(errno)));
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }
  else if (pid == 0)
  {
    // Child process
    close(pipefd[0]);                // Close the read end
    dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe
    dup2(pipefd[1], STDERR_FILENO);  // Redirect stderr to the pipe
    close(pipefd[1]);                // Close the write end

    if (execvp(args[0], args.data()) == -1)
    {
      perror("execvp");
      bld::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
      exit(EXIT_FAILURE);
    }
    abort();  // Should never reach here
  }
  else
  {
    // Parent process
    close(pipefd[1]);  // Close the write end

    std::vector<char> buffer(buffer_size);
    ssize_t bytes_read;
    output.clear();

    while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0)
      output.append(buffer.data(), bytes_read);
    close(pipefd[0]);  // Close the read end

    return bld::wait_for_process(pid) > 0;  // Use wait_for_process to handle the exit status
  }
}

bool bld::read_shell_output(const std::string &cmd, std::string &output, size_t buffer_size)
{
  if (buffer_size == 0)
  {
    bld::log(Log_type::ERROR, "Buffer size cannot be zero.");
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    bld::log(Log_type::ERROR, "Failed to create pipe: " + std::string(strerror(errno)));
    return false;
  }

  pid_t pid = fork();
  if (pid == -1)
  {
    bld::log(Log_type::ERROR, "Failed to create child process: " + std::string(strerror(errno)));
    return false;
  }
  else if (pid == 0)
  {
    close(pipefd[0]);                // Close the read end
    dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe
    dup2(pipefd[1], STDERR_FILENO);  // Redirect stderr to the pipe
    close(pipefd[1]);                // Close the write end

    std::vector<char *> args;
    args.push_back(const_cast<char *>("/bin/sh"));
    args.push_back(const_cast<char *>("-c"));
    args.push_back(const_cast<char *>(cmd.c_str()));
    args.push_back(nullptr);

    execvp(args[0], args.data());
    perror("execvp");
    exit(EXIT_FAILURE);
  }
  else
  {
    close(pipefd[1]);  // Close the write end

    std::vector<char> buffer(buffer_size);
    ssize_t bytes_read;
    output.clear();

    while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0)
      output.append(buffer.data(), bytes_read);
    close(pipefd[0]);  // Close the read end

    return wait_for_process(pid) > 0;  // Use wait_for_process instead of direct waitpid
  }
}

bool bld::is_executable_outdated(std::string file_name, std::string executable)
{
  try
  {
    // Check if the source file exists
    if (!std::filesystem::exists(file_name))
    {
      bld::log(Log_type::ERROR, "Source file does not exist: " + file_name);
      return false;  // Or handle this case differently
    }

    // Check if the executable exists
    if (!std::filesystem::exists(executable))
      return true;  // Treat as changed if the executable doesn't exist

    // Get last write times
    auto last_write_time = std::filesystem::last_write_time(file_name);
    auto last_write_time_exec = std::filesystem::last_write_time(executable);

    // Compare timestamps
    return last_write_time > last_write_time_exec;
  }
  catch (const std::filesystem::filesystem_error &e)
  {
    bld::log(Log_type::ERROR, "Filesystem error: " + std::string(e.what()));
    return false;  // Or handle the error differently
  }
  catch (const std::exception &e)
  {
    bld::log(Log_type::ERROR, std::string(e.what()));
    return false;  // Or handle the error differently
  }
}

void bld::rebuild_yourself_onchange_and_run(const std::string &filename, const std::string &executable, std::string compiler)
{
  if (bld::is_executable_outdated(filename, executable))
  {
    bld::log(Log_type::INFO, "Build executable not up-to-date. Rebuilding...");
    bld::Command cmd = {};

    // Detect the compiler if not provided
    if (compiler.empty())
    {
#ifdef __clang__
      compiler = "clang++";
#elif defined(__GNUC__)
      compiler = "g++";
#elif defined(_MSC_VER)
      compiler = "cl";  // MSVC uses 'cl' as the compiler command
#else
      bld::log(Log_type::ERROR, "Unknown compiler. Defaulting to g++.");
      compiler = "g++";
#endif
    }

    // Set up the compile command
    cmd.parts = {compiler, filename, "-o", executable};

    // Execute the compile command
    if (bld::execute(cmd) <= 0)
    {
      bld::log(Log_type::ERROR, "Failed to rebuild executable.");
      return;
    }

    bld::log(Log_type::INFO, "Rebuild successful. Restarting...");

    // Run the new executable using bld::execute
    bld::Command restart_cmd = {};
    restart_cmd.parts = {executable};
    if (bld::execute(restart_cmd) <= 0)
    {
      bld::log(Log_type::ERROR, "Failed to restart executable.");
      return;
    }

    // Exit the current process after successfully restarting
    std::exit(EXIT_SUCCESS);
  }
}

void bld::rebuild_yourself_onchange(const std::string &filename, const std::string &executable, std::string compiler)
{
  if (bld::is_executable_outdated(filename, executable))
  {
    bld::log(Log_type::INFO, "Build executable not up-to-date. Rebuilding...");
    bld::Command cmd = {};

    // Detect the compiler if not provided
    if (compiler.empty())
    {
#ifdef __clang__
      compiler = "clang++";
#elif defined(__GNUC__)
      compiler = "g++";
#elif defined(_MSC_VER)
      compiler = "cl";  // MSVC uses 'cl' as the compiler command
#else
      bld::log(Log_type::ERROR, "Unknown compiler. Defaulting to g++.");
      compiler = "g++";
#endif
    }

    // Set up the compile command
    cmd.parts = {compiler, filename, "-o", executable};

    // Execute the compile command
    if (bld::execute(cmd) <= 0)
    {
      bld::log(Log_type::ERROR, "Failed to rebuild executable.");
      return;
    }
  }
}

bool bld::args_to_vec(int argc, char *argv[], std::vector<std::string> &args)
{
  if (argc < 1)
    return false;

  args.reserve(argc - 1);
  for (int i = 1; i < argc; i++)
    args.push_back(argv[i]);

  return true;
}

bld::Config::Config() : hot_reload_files(), cmd_args()
{
  hot_reload = false;
  extra_args = true;
  verbose = false;
  override_run = false;
  compiler = "";
  target_executable = "";
  target_platform = "";
  build_dir = "build";
  compiler_flags = "";
  linker_flags = "";
  pre_build_command = "";
  post_build_command = "";
  default_run_args = "";

  init();
  // Automatically load configuration if the file exists
  if (std::filesystem::exists("build.conf"))
    load_from_file("build.conf");
}
bld::Config &bld::Config::get()
{
  static Config instance;
  return instance;
}

void bld::Config::init()
{
#ifdef _WIN32
  target_platform = "win32";
#elif defined(__APPLE__)
  target_platform = "darwin";
#elif defined(__linux__)
  target_platform = "linux";
#else
  target_platform = "unknown";
#endif

#ifdef __clang__
  compiler = "clang++";
#elif defined(__GNUC__)
  compiler = "g++";
#elif defined(_MSC_VER)
  compiler = "cl";
#else
  compiler = "g++";
#endif
}

bool bld::Config::load_from_file(const std::string &filename)
{
  if (!std::filesystem::exists(filename))
  {
    bld::log(bld::Log_type::WARNING, "Config file not found: " + filename);
    return false;
  }

  std::ifstream file(filename);
  if (!file.is_open())
  {
    bld::log(bld::Log_type::ERROR, "Failed to open config file: " + filename);
    return false;
  }

  std::string line;
  while (std::getline(file, line))
  {
    if (line.empty() || line[0] == '#')
      continue;

    std::stringstream ss(line);
    std::string key, value;
    std::getline(ss, key, '=');
    std::getline(ss, value);

    if (key == "hot_reload")
      hot_reload = (value == "true");
    else if (key == "compiler")
      compiler = value;
    else if (key == "target")
      target_executable = value;
    else if (key == "platform")
      target_platform = value;
    else if (key == "build_dir")
      build_dir = value;
    else if (key == "compiler_flags")
      compiler_flags = value;
    else if (key == "linker_flags")
      linker_flags = value;
    else if (key == "verbose")
      verbose = (value == "true");
    else if (key == "pre_build_command")
      pre_build_command = value;
    else if (key == "post_build_command")
      post_build_command = value;
    else if (key == "default_run_args")
      default_run_args = value;
    else if (key == "override_run")
      override_run = (value == "true");
    else if (key == "hot_reload_files")
    {
      hot_reload_files.clear();
      std::stringstream fs(value);
      std::string file;
      while (std::getline(fs, file, ','))
        hot_reload_files.push_back(file);
    }
    else
    {
      bld::log(bld::Log_type::WARNING, "Unknown key in config file: " + key);
    }
  }
  return true;
}

bool bld::Config::save_to_file(const std::string &filename)
{
  std::ofstream file(filename);
  if (!file.is_open())
    return false;

  if (hot_reload)
    file << "hot_reload=true\n";
  if (!compiler.empty())
    file << "compiler=" << compiler << "\n";
  if (!target_executable.empty())
    file << "target=" << target_executable << "\n";
  if (!target_platform.empty())
    file << "platform=" << target_platform << "\n";
  if (!build_dir.empty())
    file << "build_dir=" << build_dir << "\n";
  if (!compiler_flags.empty())
    file << "compiler_flags=" << compiler_flags << "\n";
  if (!linker_flags.empty())
    file << "linker_flags=" << linker_flags << "\n";
  if (verbose)
    file << "verbose=true\n";
  if (!pre_build_command.empty())
    file << "pre_build_command=" << pre_build_command << "\n";
  if (!post_build_command.empty())
    file << "post_build_command=" << post_build_command << "\n";
  if (!default_run_args.empty())
    file << "default_run_args=" << default_run_args << "\n";
  if (override_run)
    file << "override_run=true\n";

  if (!hot_reload_files.empty())
  {
    file << "files=";
    for (size_t i = 0; i < hot_reload_files.size(); ++i)
    {
      file << hot_reload_files[i];
      if (i < hot_reload_files.size() - 1)
        file << ",";
    }
    file << "\n";
  }

  return true;
}
#endif
