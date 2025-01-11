#pragma once
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <vector>

enum class Log_type
{
  INFO,
  WARNING,
  ERROR
};

namespace utl
{
  void notify(Log_type type, const std::string &msg);
}

struct Command
{
  std::vector<std::string> parts;

  Command() = default;

  // Constructor to initialize with a list of strings
  template <typename... Args>
  Command(Args... args)
  {
    (parts.emplace_back(args), ...);
  }

  // Add more parts to the command
  template <typename... Args>
  void add_parts(Args... args)
  {
    (parts.emplace_back(args), ...);
  }

  // Get the full command as a single string
  std::string get_command_string() const
  {
    std::stringstream ss;
    for (const auto &part : parts)
      ss << part << " ";
    return ss.str();
  }

  // Convert parts to C-style arguments for `execvp`
  std::vector<char *> to_exec_args() const
  {
    std::vector<char *> exec_args;
    for (const auto &part : parts)
      exec_args.push_back(const_cast<char *>(part.c_str()));
    exec_args.push_back(nullptr);  // Null terminator
    return exec_args;
  }

  bool empty() const { return parts.empty(); }
};

int execute(const Command &command)
{
  if (command.empty())
  {
    utl::notify(Log_type::ERROR, "No command to execute.");
    return -1;
  }

  auto args = command.to_exec_args();
  utl::notify(Log_type::INFO, "Executing command: " + command.get_command_string());

  pid_t pid = fork();
  if (pid == -1)
  {
    utl::notify(Log_type::ERROR, "Failed to create child process.");
    return -1;
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
        utl::notify(Log_type::ERROR, "Command failed with exit code: " + std::to_string(exit_code));
        return -1;
      }
    }
    else if (WIFSIGNALED(status))
    {
      int signal = WTERMSIG(status);
      utl::notify(Log_type::ERROR, "Command terminated by signal: " + std::to_string(signal));
      return -1;
    }
  }

  utl::notify(Log_type::INFO, "Command executed successfully.");
  return 0;
}

#ifdef B_LDR_IMPLEMENTATION
#include <iostream>
void utl::notify(Log_type type, const std::string &msg)
{
  switch (type)
  {
    case Log_type::INFO:
      std::clog << "[INFO]: " << msg << std::endl;
      break;
    case Log_type::WARNING:
      std::clog << "[WARNING]: " << msg << std::endl;
      break;
    case Log_type::ERROR:
      std::clog << "[ERROR]: " << msg << std::endl;
      break;
    default:
      std::clog << "[UNKNOWN]: " << msg << std::endl;
      break;
  }
}
#endif
