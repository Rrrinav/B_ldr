#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
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
  void notify(Log_type type, const std::string &msg)
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

}  // namespace utl

struct Bldr
{
  std::vector<std::string> commands;

  template <typename... Args>
  void push_commands(Args... args)
  {
    (commands.push_back(args), ...);
  }

  int execute()
  {
    if (commands.empty())
    {
      utl::notify(Log_type::ERROR, "No commands to execute.");
      return -1;
    }

    // Convert commands to `char*` array for `execvp`
    std::vector<char *> args;
    for (auto &cmd : commands)
      args.push_back(cmd.data());
    args.push_back(nullptr);  // Null-terminate the array

    // Create a child process
    pid_t pid = fork();

    if (pid == -1)
    {
      // Fork failed
      utl::notify(Log_type::ERROR, "Failed to create child process.");
      return -1;
    }
    else if (pid == 0)
    {
      // Child process
      utl::notify(Log_type::INFO, "Executing command:");
      for (const auto &cmd : commands)
        std::cout << cmd << " ";
      std::cout << std::endl;

      // Execute the command
      if (execvp(args[0], args.data()) == -1)
      {
        perror("execvp");     // Print error message
        _exit(EXIT_FAILURE);  // Exit child process
        return -1;
      }
    }
    else
    {
      // Parent process
      int status;
      waitpid(pid, &status, 0);  // Wait for the child process to complete

      if (WIFEXITED(status))
      {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0)
        {
          utl::notify(Log_type::INFO, "Command executed successfully.");
          return 1;
        }
        else
        {
          utl::notify(Log_type::ERROR, "Command failed with exit code: " + std::to_string(exit_code));
          return -1;
        }
      }
      else if (WIFSIGNALED(status))
      {
        int signal = WTERMSIG(status);
        utl::notify(Log_type::ERROR, "Command terminated by signal: " + std::to_string(signal));
        return 0;
      }
    }
    return 1;
  }

  void reset_commands() { commands.clear(); }
};

int main(int argc, char **argv)
{
  std::string output = "main";
  Bldr b;
  b.push_commands("g++", "main.cpp", "-o", output, "-Wall");
  // Execute the command
  b.execute();
  return 0;
}
