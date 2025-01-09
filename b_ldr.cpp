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

    for (const auto &cmd : commands)
    {
      std::vector<char *> args = {const_cast<char *>(cmd.c_str()), nullptr};

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
          _exit(EXIT_FAILURE);
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
    }

    utl::notify(Log_type::INFO, "All commands executed successfully.");
    return 0;
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
