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
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace b_ldr
{
  // Log type is enumeration for b_ldr::function to show type of log
  enum class Log_type
  {
    INFO,
    WARNING,
    ERROR
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

    // @return ( std::string ): Get the full command as a single string
    // @description: Get the full command as a single string
    std::string get_command_string() const;

    // @return ( std::vector<char *> ): Get the full command but as C-style arguments for `execvp`
    // @description: Get the full command as a C-style arguments for sys calls
    std::vector<char *> to_exec_args() const;

    // @return ( bool ): Check if the command is empty
    bool is_empty() const { return parts.empty(); }

    // @return ( std::string ): Get the command as a printable string wrapped in single quotes
    std::string get_print_string() const;
  };

  // @beief: Validate the command before executing
  // @param command ( Command ): Command to validate
  bool validate_command(const Command &command);

  // @brief: Wait for the process to complete
  // @param pid ( pid_t ): Process ID to wait for
  // @description: Wait for the process to complete and log the status. Use this function instead of direct waitpid
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

  // @brief: Preprocess the command for shell execution
  // @param cmd ( Command ): Command to preprocess
  // @return: Preprocessed command for shell execution
  // @description: Preprocess the command for shell execution by adding shell command and arguments
  //    Windows:     cmd.exe /c <command>
  //    Linux/macOS: /bin/sh -c <command>
  Command preprocess_commands_for_shell(const Command &cmd);

  // @brief: Execute the shell command with preprocessed parts
  // param cmd ( Command ): Command to execute in shell
  // @description: Execute the shell command with preprocessed parts
  //    Uses execute function to execute the preprocessed command
  //    return the return value of the execute function
  int execute_shell(std::string command);

  // @brief: Execute the shell command with preprocessed parts but asks wether to execute or not first
  // @param cmd ( Command ): Command to execute in shell
  // @param prompt ( bool ): Ask for confirmation before executing
  // @description: Execute the shell command with preprocessed parts with prompting
  //    Uses execute function to execute the preprocessed command
  //    return the return value of the execute function
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

  bool is_executable_outdated(std::string file_name, std::string executable);
}  // namespace b_ldr

#ifdef B_LDR_IMPLEMENTATION

#include <iostream>
#include <ostream>
#include <sstream>

void b_ldr::log(Log_type type, const std::string &msg)
{
  switch (type)
  {
    case Log_type::INFO:
      std::cout << "  [INFO]: " << msg << std::endl;
      break;
    case Log_type::WARNING:
      std::cout << "  [WARNING]: " << msg << std::endl;
      break;
    case Log_type::ERROR:
      std::cerr << "  [ERROR]: " << msg << std::flush << std::endl;
      break;
    default:
      std::cout << "  [UNKNOWN]: " << msg << std::endl;
      break;
  }
}

// Get the full command as a single string
std::string b_ldr::Command::get_command_string() const
{
  std::stringstream ss;
  for (const auto &part : parts)
    ss << part << " ";
  return ss.str();
}

// Convert parts to C-style arguments for `execvp`
std::vector<char *> b_ldr::Command::to_exec_args() const
{
  std::vector<char *> exec_args;
  for (const auto &part : parts)
    exec_args.push_back(const_cast<char *>(part.c_str()));
  exec_args.push_back(nullptr);  // Null terminator
  return exec_args;
}

std::string b_ldr::Command::get_print_string() const
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
b_ldr::Command::Command(Args... args)
{
  (parts.emplace_back(args), ...);
}

template <typename... Args>
void b_ldr::Command::add_parts(Args... args)
{
  (parts.emplace_back(args), ...);
}

bool b_ldr::validate_command(const b_ldr::Command &command)
{
  b_ldr::log(b_ldr::Log_type::WARNING, "Do you want to execute " + command.get_print_string() + "in shell");
  std::cerr << "  [WARNING]: Answer[y/n]: ";
  std::string response;
  std::getline(std::cin, response);
  return (response == "y" || response == "Y");
}

int b_ldr::wait_for_process(pid_t pid)
{
  int status;
  waitpid(pid, &status, 0);  // Wait for the process to complete

  if (WIFEXITED(status))
  {
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0)
    {
      b_ldr::log(b_ldr::Log_type::ERROR, "Process exited with non-zero status: " + std::to_string(exit_code));
      return exit_code;  // Return exit code for failure
    }
    b_ldr::log(b_ldr::Log_type::INFO, "Process exited successfully.");
  }
  else if (WIFSIGNALED(status))
  {
    int signal = WTERMSIG(status);
    b_ldr::log(b_ldr::Log_type::ERROR, "Process terminated by signal: " + std::to_string(signal));
    return -1;  // Indicate signal termination
  }
  else
  {
    b_ldr::log(b_ldr::Log_type::WARNING, "Unexpected process termination status.");
  }

  return static_cast<int>(pid);  // Indicate success
}

int b_ldr::execute(const Command &command)
{
  if (command.is_empty())
  {
    b_ldr::log(Log_type::ERROR, "No command to execute.");
    return -1;
  }

  auto args = command.to_exec_args();
  b_ldr::log(Log_type::INFO, "Executing command: " + command.get_print_string());

  pid_t pid = fork();
  if (pid == -1)
  {
    b_ldr::log(Log_type::ERROR, "Failed to create child process.");
    return 0;
  }
  else if (pid == 0)
  {
    // Child process
    if (execvp(args[0], args.data()) == -1)
    {
      perror("execvp");
      b_ldr::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
      exit(EXIT_FAILURE);
    }
    // This line should never be reached
    b_ldr::log(Log_type::ERROR, "Unexpected code execution after execvp. Did we find a bug? in libc or kernel?");
    abort();
  }

  return b_ldr::wait_for_process(pid);  // Use wait_for_process instead of direct waitpid
}

void b_ldr::print_metadata()
{
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

b_ldr::Command b_ldr::preprocess_commands_for_shell(const b_ldr::Command &cmd)
{
  b_ldr::Command cmd_s;

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
int b_ldr::execute_shell(std::string cmd)
{
  // Preprocess the command for shell execution
  auto cmd_s = preprocess_commands_for_shell(cmd);

  // Execute the shell command using the original execute function
  return execute(cmd_s);
}

int b_ldr::execute_shell(std::string cmd, bool prompt)
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

bool b_ldr::read_process_output(const Command &cmd, std::string &output, size_t buffer_size)
{
  if (cmd.is_empty())
  {
    b_ldr::log(Log_type::ERROR, "No command to execute.");
    return false;
  }

  if (buffer_size == 0)
  {
    b_ldr::log(Log_type::ERROR, "Buffer size cannot be zero.");
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    b_ldr::log(Log_type::ERROR, "Failed to create pipe: " + std::string(strerror(errno)));
    return false;
  }

  auto args = cmd.to_exec_args();
  b_ldr::log(Log_type::INFO, "Extracting output from: " + cmd.get_print_string());

  pid_t pid = fork();
  if (pid == -1)
  {
    b_ldr::log(Log_type::ERROR, "Failed to create child process: " + std::string(strerror(errno)));
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
      b_ldr::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
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

    return b_ldr::wait_for_process(pid) > 0;  // Use wait_for_process to handle the exit status
  }
}

bool b_ldr::read_shell_output(const std::string &cmd, std::string &output, size_t buffer_size)
{
  if (buffer_size == 0)
  {
    b_ldr::log(Log_type::ERROR, "Buffer size cannot be zero.");
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    b_ldr::log(Log_type::ERROR, "Failed to create pipe: " + std::string(strerror(errno)));
    return false;
  }

  pid_t pid = fork();
  if (pid == -1)
  {
    b_ldr::log(Log_type::ERROR, "Failed to create child process: " + std::string(strerror(errno)));
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

bool b_ldr::is_executable_outdated(std::string file_name, std::string executable)
{
  try
  {
    // Check if the source file exists
    if (!std::filesystem::exists(file_name))
    {
      b_ldr::log(Log_type::ERROR, "Source file does not exist: " + file_name);
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
    b_ldr::log(Log_type::ERROR, "Filesystem error: " + std::string(e.what()));
    return false;  // Or handle the error differently
  }
  catch (const std::exception &e)
  {
    b_ldr::log(Log_type::ERROR, std::string(e.what()));
    return false;  // Or handle the error differently
  }
}
#endif
