#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>   // For errno
#include <cstring>  // For strerror
#include <iostream>
#include <string>
#include <vector>

bool read_output(const std::string &cmd, std::string &output, size_t buffer_size = 4096)
{
  if (buffer_size == 0)
  {
    std::cerr << "Buffer size cannot be zero." << std::endl;
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    std::cerr << "Failed to create pipe: " << std::strerror(errno) << std::endl;
    return false;
  }

  pid_t pid = fork();
  if (pid == -1)
  {
    std::cerr << "Failed to create child process: " << std::strerror(errno) << std::endl;
    return false;
  }
  else if (pid == 0)
  {
    close(pipefd[0]);                // Close the read end
    dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe
    dup2(pipefd[1], STDERR_FILENO);  // Redirect stderr to the pipe (optional)
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

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
    {
      int exit_code = WEXITSTATUS(status);
      if (exit_code != 0)
      {
        std::cerr << "Command failed with exit code: " << exit_code << std::endl;
        return false;
      }
    }
    else if (WIFSIGNALED(status))
    {
      int signal = WTERMSIG(status);
      std::cerr << "Command terminated by signal: " << signal << std::endl;
      return false;
    }
  }

  return true;
}

int main()
{
  std::string command = "pkg-config --libs raylib";  // Command to execute
  std::string command2 = "ls -l";                    // Command to execute
  std::string output;

  if (read_output(command2, output, 1))
    std::cout << "Command Output:\n" << output << std::endl;
  else
    std::cerr << "Failed to execute command." << std::endl;

  return 0;
}
