#pragma once
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
   * @param command [Command]: Command to execute
   * @return: returns a code to indicate success or failure
   *    1: Command executed successfully
   *    0: Command failed to execute
   *   -1: No command to execute
   * @description: Execute the command and log the status alongwith
   */
  int execute(const Command &command);
}  // namespace b_ldr

#ifdef B_LDR_IMPLEMENTATION

#include <iostream>
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
  return 1;
}
#endif
