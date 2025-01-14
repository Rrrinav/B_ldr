# Builder (b_ldr)

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

To use `b_ldr` in your project, include the `b_ldr.hpp` header file and define `B_LDR_IMPLEMENTATION` in one of your source files to include the implementation.

```cpp
#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"
```

## Usage

### Logging

Log messages with different severity levels:

```cpp
b_ldr::log(b_ldr::Log_type::INFO, "This is an info message.");
b_ldr::log(b_ldr::Log_type::WARNING, "This is a warning message.");
b_ldr::log(b_ldr::Log_type::ERROR, "This is an error message.");
```

### Command Execution

Create and execute commands:

```cpp
b_ldr::Command cmd("ls", "-la");
int result = b_ldr::execute(cmd);
```

Execute shell commands:

```cpp
std::string shell_cmd = "echo Hello, World!";
int result = b_ldr::execute_shell(shell_cmd);
```

### Process Output

Capture the output of a command:

```cpp
std::string output;
b_ldr::Command cmd("ls", "-la");
bool success = b_ldr::read_process_output(cmd, output);
```

Capture the output of a shell command:

```cpp
std::string output;
std::string shell_cmd = "echo Hello, World!";
bool success = b_ldr::read_shell_output(shell_cmd, output);
```

### System Metadata

Print system metadata:

```cpp
b_ldr::print_metadata();
```


## Author

Rinav (GitHub: [rrrinav](https://github.com/rrrinav))
