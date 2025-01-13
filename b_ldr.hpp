#pragma once
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

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

  /* @param type [Log_type enum ]: Type of log
   * @param msg [std::string]: Message to log
   * @description: Function to log messages with type
   */
  void log(Log_type type, const std::string &msg);

  // Struct to hold command parts
  struct Command
  {
    std::vector<std::string> parts;  // > parts of the command

    // Default constructor
    Command() : parts{} {}

    // @tparam args [variadic template]: Command parts
    template <typename... Args>
    Command(Args... args);

    // @tparam args [variadic template]: Command parts
    template <typename... Args>
    void add_parts(Args... args);

    // @return [std::string]: Get the full command as a single string
    // @description: Get the full command as a single string
    std::string get_command_string() const;

    // @return [std::vector<char *>]: Get the full command but as C-style arguments for `execvp`
    // @description: Get the full command as a C-style arguments for sys calls
    std::vector<char *> to_exec_args() const;

    // @return [bool]: Check if the command is empty
    bool is_empty() const { return parts.empty(); }
  };

  /* @brief: Execute the command
   * @param command [Command]: Command to execute, must be a valid process command and not shell command
   * @return: returns a code to indicate success or failure
   *   >0 : Command executed successfully, returns pid of fork.
   *    0 : Command failed to execute or something wrong on system side
   *   -1 : No command to execute or something wrong on user side
   * @description: Execute the command using fork and log the status alongwith
   */
  int execute(const Command &command);
  void print_metadata();
  // Preprocess the command to add shell invocation
  Command preprocess_commands_for_shell(const Command &cmd);

  // Execute the shell command with preprocessed parts
  int execute_shell(Command cmd);
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
      std::clog << "  [INFO]: " << msg << std::endl;
      break;
    case Log_type::WARNING:
      std::clog << "  [WARNING]: " << msg << std::endl;
      break;
    case Log_type::ERROR:
      std::clog << "  [ERROR]: " << msg << std::endl;
      break;
    default:
      std::clog << "  [UNKNOWN]: " << msg << std::endl;
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

int b_ldr::execute(const Command &command)
{
  if (command.is_empty())
  {
    b_ldr::log(Log_type::ERROR, "No command to execute.");
    return -1;
  }

  auto args = command.to_exec_args();
  b_ldr::log(Log_type::INFO, "Executing command: " + command.get_command_string());

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
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    // Parent process
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
    {
      int exit_code = WEXITSTATUS(status);
      if (exit_code != 0)
      {
        b_ldr::log(Log_type::ERROR, "Command failed with exit code: " + std::to_string(exit_code));
        return 0;
      }
    }
    else if (WIFSIGNALED(status))
    {
      int signal = WTERMSIG(status);
      b_ldr::log(Log_type::ERROR, "Command terminated by signal: " + std::to_string(signal));
      return 0;
    }
  }

  b_ldr::log(Log_type::INFO, "Command executed successfully.");
  return static_cast<int>(pid);
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
int b_ldr::execute_shell(b_ldr::Command cmd)
{
  // Preprocess the command for shell execution
  auto cmd_s = preprocess_commands_for_shell(cmd);

  // Execute the shell command using the original execute function
  return execute(cmd_s);
}
#endif
