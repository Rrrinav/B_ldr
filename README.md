# Builder (bld)

A simple C++ build system that uses C++ as the scripting language thus doesn't need any new tools to be installed.
Moreover, it is easier because well, it uses C++ as a scripting language.

Because [Tsoding](github.com/tsoding) said you should write your own build system.


## Features

- **Command Execution**: Execute system commands and shell commands with ease.
- **Logging**: Log messages with different severity levels (INFO, WARNING, ERROR).
- **Process Management**: Wait for processes to complete and handle their exit statuses.
- **Output Handling**: Capture and read the output of executed commands.
- **System Metadata**: Print system metadata including OS, compiler, and architecture information.


## Installation

To use `bld` in your project, include the `bld.hpp` header file and define `B_LDR_IMPLEMENTATION` in one of your source files to include the implementation.

```cpp
#define B_LDR_IMPLEMENTATION
#include "bld.hpp"
```

## Usage

### Logging

Log messages with different severity levels:

```cpp
bld::log(bld::Log_type::INFO, "This is an info message.");
bld::log(bld::Log_type::WARNING, "This is a warning message.");
bld::log(bld::Log_type::ERROR, "This is an error message.");
```

### Command Execution

Create and execute commands:

```cpp
bld::Command cmd("ls", "-la");
int result = bld::execute(cmd);
```

Execute shell commands:

```cpp
std::string shell_cmd = "echo Hello, World!";
int result = bld::execute_shell(shell_cmd);
```

### Process Output

Capture the output of a command:

```cpp
std::string output;
bld::Command cmd("ls", "-la");
bool success = bld::read_process_output(cmd, output);
```

Capture the output of a shell command:

```cpp
std::string output;
std::string shell_cmd = "echo Hello, World!";
bool success = bld::read_shell_output(shell_cmd, output);
```

### incremental

```cpp
int main(int argc, char *argv[])
{
  BLD_REBUILD_YOURSELF_ONCHANGE();

  bld::Dep_graph dg;

  dg.add_dep({"main",
             {"main.cpp", "./foo.o", "./bar.o"},
             {"g++", "main.cpp", "-o", "main", "foo.o", "bar.o"}});

  dg.add_dep({"./foo.o",
             {"foo.cpp"},
             {"g++", "-c", "foo.cpp", "-o", "foo.o"}});

  dg.add_dep({"./bar.o",
             {"bar.cpp"},
             {"g++", "-c", "bar.cpp", "-o", "bar.o"}});

  dg.build_all();

  return 0;
}
```
```bash
$ls
main.cpp foo.cpp foo.hpp bar.cpp bar.hpp

```

### File System

Check if an executable is up-to-date with it's file:
    This specific example tracks it's own executable file.

```cpp
 // Requires arguments for main function
int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();
}
```

### System Metadata

Print system metadata:

```cpp
bld::print_metadata();
```

## TODO

- [X] Parallel incremental builds
- [ ] Fully cross-platform functions

## Author

Rinav (GitHub: [rrrinav](https://github.com/rrrinav))
