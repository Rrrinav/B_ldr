`bld` Library Documentation
===========================

Overview
--------

The `bld` library is a header-only C++ library designed to simplify build automation and process management. It provides utilities for rebuilding executables, handling command-line arguments, executing commands, and managing configurations. Inspired by the STB style, the library is simple, lightweight, and easy to integrate into projects.

Features
--------

-   **Rebuild Executables**: Automatically rebuild executables if the source file is newer than the executable.
-   **Command Execution**: Execute shell commands and process commands with support for parallel execution.
-   **Configuration Management**: Manage build configurations using a singleton `Config` class.
-   **File System Utilities**: Perform common file system operations like reading, writing, copying, and moving files.
-   **Logging**: Log messages with different log levels (INFO, WARNING, ERROR, DEBUG).
-   **Command-Line Argument Handling**: Handle command-line arguments for common build tasks like running and configuring.

Usage
-----

### Including the Library

To use the `bld` library, simply include the header file in your project:

```cpp
#define B_LD_IMPLEMENTATION
#include "b_ldr.h"

```

### Macros

-   **`BLD_REBUILD_YOURSELF_ONCHANGE()`**: Rebuild the executable if the source file is newer and run it.
-   **`C_BLD_REBUILD_YOURSELF_ONCHANGE(compiler)`**: Rebuild the executable with a specified compiler.
-   **`BLD_HANDLE_ARGS()`**: Handle command-line arguments.

### Example

``` cpp
#include "b_ldr.h"

int main(int argc, char *argv[]) {
    BLD_HANDLE_ARGS(); // Handle command-line arguments
    BLD_REBUILD_YOURSELF_ONCHANGE(); // Rebuild and run if necessary
    return 0;
}

```

API Reference
-------------

### Logging

-   **`void log(Log_type type, const std::string &msg)`**: Log a message with a specified log type (INFO, WARNING, ERROR, DEBUG).

### Command Execution

-   **`struct Command`**: Holds command parts.
    -   **`std::string get_command_string() const`**: Get the full command as a single string.
    -   **`std::vector<char *> to_exec_args() const`**: Get the command as C-style arguments for `execvp`.
    -   **`bool is_empty() const`**: Check if the command is empty.
    -   **`std::string get_print_string() const`**: Get the command as a printable string.
-   **`int execute(const Command &command)`**: Execute a command and wait for it to complete.

-   **`int execute_without_wait(const Command &command)`**: Execute a command without waiting for it to complete.

-   **`Exec_par_result execute_parallel(const std::vector<Command> &cmds, size_t threads, bool strict)`**: Execute multiple commands in parallel.
        - `Exec_par_result`: A struct holding number of successful commands and indices of failed commands.
            - size_t completed;                    // Number of successfully completed commands
            - std::vector<size_t> failed_indices;  // Indices of commands that failed

### Configuration Management

-   **`class Config`**: A singleton class to manage build configurations.
    -   **`static Config &get()`**: Get the singleton instance of the `Config` class.
    -   **`void init()`**: Initialize the configuration with default values.
    -   **`bool load_from_file(const std::string &filename)`**: Load configuration from a file. Loads automatically if the file exists.
    -   **`bool save_to_file(const std::string &filename)`**: Save configuration to a file. Need to save to file manually if you're changing config using code.

### File System Utilities

The **`fs`** namespace provides utilities for file system operations:

-   **`bool read_file(const std::string &path, std::string &content)`**: Read the entire content of a file into a string.
-   **`bool write_entire_file(const std::string &path, const std::string &content)`**: Write content to a file.
-   **`bool append_file(const std::string &path, const std::string &content)`**: Append content to a file.
-   **`bool read_lines(const std::string &path, std::vector<std::string> &lines)`**: Read a file line by line.
-   **`bool replace_in_file(const std::string &path, const std::string &from, const std::string &to)`**: Replace text in a file.
-   **`bool copy_file(const std::string &from, const std::string &to, bool overwrite = false)`**: Copy a file.
-   **`bool move_file(const std::string &from, const std::string &to)`**: Move or rename a file.
-   **`std::string get_extension(const std::string &path)`**: Get the file extension.
-   **`bool create_directory(const std::string &path)`**: Create a directory.
-   **`bool create_dir_if_not_exists(const std::string &path)`**: Create a directory if it doesn't exist.
-   **`bool create_dirs_if_not_exists(const Paths&... paths)`**: Creates multiple directories if they don't exist.
-   **`bool remove_dir(const std::string &path)`**: Remove a directory and its contents.
-   **`std::vector<std::string> list_files_in_dir(const std::string &path, bool recursive = false)`**: List files in a directory.
-   **`std::vector<std::string> list_directories(const std::string &path, bool recursive = false)`**: List directories in a directory.
-   **`std::string get_file_name(std::string full_path);`**: Get the file name from the full path.

-   **`std::string strip_file_name(std::string full_path);`**: Strip off the file name from the full path thus leaving behind the directory name.

### environment variables

The `env` namespace provides utilities for environment variable operations:

-   **`std::string get(const std::string &key)`**: Get the value of env variable
-   **`bool set(const std::string &key, const std::string &value);`**: Set an environment variable
-   **`bool exists(const std::string &key);`**: Check if an env variable exists
-   **`bool unset(const std::string &key);`**: Unset an env variable
-   **`std::unordered_map<std::string, std::string> get_all();`**: Get a map of all the environment variables

### Strings

The namespace `str` under `bld` provides these string functions

- `std::string trim(const std::string &str)`: Removes leading and trailing whitespace from a string, including: `' '`, `\t`, `\n`, `\r`, `\f`, `\v`.

- `std::string trim_left(const std::string &str)`: Removes leading whitespace from a string.

- `std::string trim_right(const std::string &str)`: Removes trailing whitespace from a string.

- `std::string to_lower(const std::string &str)`: Converts a string to lowercase.

- `std::string to_upper(const std::string &str)`: Converts a string to uppercase.

- `std::string replace(std::string str, const std::string &from, const std::string &to)`: Replaces all occurrences of a substring `from` with another substring `to` within `str`.

- `bool starts_with(const std::string &str, const std::string &prefix)`: Checks if `str` starts with `prefix`.

- `bool ends_with(const std::string &str, const std::string &suffix)`: Checks if `str` ends with `suffix`.

- `std::string join(const std::vector<std::string> &strs, const std::string &delimiter)`: Joins a vector of strings into a single string separated by `delimiter`.

- `std::string trim_till(const std::string &str, char delimiter)`: Trims `str` until the first occurrence of `delimiter`.

- `bool equal_ignorecase(const std::string &str1, const std::string &str2)`: Checks if two strings are equal, ignoring case.

- `std::vector<std::string> chop_by_delimiter(const std::string &s, const std::string &delimiter)`: Splits `s` by `delimiter` and returns a vector of the substrings.

- `std::string remove_duplicates(const std::string &str)`: Removes duplicate characters from `str`.

- `std::string remove_duplicates_case_insensitive(const std::string &str)`: Removes duplicate characters from `str` in a case-insensitive manner.

- `bool is_numeric(const std::string &str)`: Checks if `str` is a valid number. Handles integers and floating-point numbers, supports positive and negative numbers, and ensures only one decimal point.

- `std::string replace_all(const std::string &str, const std::string &from, const std::string &to)`: Replaces all occurrences of `from` with `to` in `str`. Handles multiple replacements efficiently, supports edge cases, and returns the original string if `from` is not found.

- `std::string trim(const std::string &str)`: Removes leading and trailing whitespace from a string, including: `' '`, `\t`, `\n`, `\r`, `\f`, `\v`.

- `std::string trim_left(const std::string &str)`: Removes leading whitespace from a string.

- `std::string trim_right(const std::string &str)`: Removes trailing whitespace from a string.

- `std::string to_lower(const std::string &str)`: Converts a string to lowercase.

- `std::string to_upper(const std::string &str)`: Converts a string to uppercase.

- `std::string replace(std::string str, const std::string &from, const std::string &to)`: Replaces all occurrences of a substring `from` with another substring `to` within `str`.

- `bool starts_with(const std::string &str, const std::string &prefix)`: Checks if `str` starts with `prefix`.

- `bool ends_with(const std::string &str, const std::string &suffix)`: Checks if `str` ends with `suffix`.

- `std::string join(const std::vector<std::string> &strs, const std::string &delimiter)`: Joins a vector of strings into a single string separated by `delimiter`.

- `std::string trim_till(const std::string &str, char delimiter)`: Trims `str` until the first occurrence of `delimiter`.

- `bool equal_ignorecase(const std::string &str1, const std::string &str2)`: Checks if two strings are equal, ignoring case.

- `std::vector<std::string> chop_by_delimiter(const std::string &s, const std::string &delimiter)`: Splits `s` by `delimiter` and returns a vector of the substrings.

- `std::string remove_duplicates(const std::string &str)`: Removes duplicate characters from `str`.

- `std::string remove_duplicates_case_insensitive(const std::string &str)`: Removes duplicate characters from `str` in a case-insensitive manner.

- `bool is_numeric(const std::string &str)`: Checks if `str` is a valid number. Handles integers and floating-point numbers, supports positive and negative numbers, and ensures only one decimal point.

- `std::string replace_all(const std::string &str, const std::string &from, const std::string &to)`: Replaces all occurrences of `from` with `to` in `str`. Handles multiple replacements efficiently, supports edge cases, and returns the original string if `from` is not found.


### Command-Line Argument Handling

-   **`void handle_args(int argc, char *argv[])`**: Handle command-line arguments.
-   **`int handle_run_command(std::vector<std::string> args)`**: Handle the run command.
-   **`void handle_config_command(std::vector<std::string> args, std::string name)`**: Handle the config command.

### Miscellaneous

-   **`void print_metadata()`**: Print system metadata (OS, compiler, architecture).
-   **`bool is_executable_outdated(const std::string &file_name, const std::string &executable)`**: Check if the executable is outdated.
-   **`void rebuild_yourself_onchange(const std::string &filename, const std::string &executable, std::string compiler)`**: Rebuild the executable if the source file is newer.
-   **`void rebuild_yourself_onchange_and_run(const std::string &filename, const std::string &executable, std::string compiler)`**: Rebuild the executable and run it if the source file is newer.

License
-------

The `bld` library is released under the MIT License. See the header file for the full license text.

Contributing
------------

Contributions are welcome! Please open an issue or submit a pull request on the GitHub repository.

Author
------

**Rinav** [ GitHub ](github.com/rrrinav)
