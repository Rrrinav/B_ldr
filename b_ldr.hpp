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

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <clocale>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* @brief: Rebuild the build executable if the source file is newer than the executable and run it
 * @description: Takes no parameters and calls bld::rebuild_yourself_onchange_and_run() with the current file and executable.
 *    bld::rebuild_yourself_onchange() detects compiler itself (g++, clang++, cl supported) and rebuilds the executable
 *    if the source file is newer than the executable. It then restarts the new executable and exits the current process.
 * If you want more cntrol, use bld::rebuild_yourself_onchange() or bld::rebuild_yourself_onchange_and_run() directly.
 */
#define BLD_REBUILD_YOURSELF_ONCHANGE() bld::rebuild_yourself_onchange_and_run(__FILE__, argv[0])
// Same but with compiler specified...
#define C_BLD_REBUILD_YOURSELF_ONCHANGE(compiler) bld::rebuild_yourself_onchange_and_run(__FILE__, argv[0], compiler)

/* @brief: Handle command-line arguments
 * @description: Takes argc and argv and calls bld::handle_args() with them.
 *    Right now, it only handles 'run' and 'config' commands.
 *    Read the documentation of function for more information.
 */
#define BLD_HANDLE_ARGS() bld::handle_args(argc, argv)

#define BLD_REBUILD_AND_ARGS()       \
    BLD_REBUILD_YOURSELF_ONCHANGE(); \
    BLD_HANDLE_ARGS()

/* File to save the configuration to.
 * You can change this to any file name you want.
 */
#define BLD_DEFAULT_CONFIG_FILE "build.conf"

/*
 * INFO:The BLD_USE_CONFIG macro is used to enable or disable the configuration system in your build tool. When BLD_USE_CONFIG is defined, the Config class and its related
 * functionality are included in the program because you can't access singleton using Config::get().
 * When BLD_USE_CONFIG is not defined, the configuration system is disabled, and the program operates without it.
 * You can define your own configuration system or use the Config class directly.
 *
 * I dont know why I didn't wrap whole class in the macro. Probably because I want to extend it later to not just be a singleton.
*/

namespace bld
{
    // Log type is enumeration for bld::function to show type of loc>
    enum class Log_type
    {
        INFO,
        WARNING,
        ERROR,
        DEBUG
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

        /* @return ( std::string ): Get the full command as a single string
         * @description: Get the full command as a single string
         */
        std::string get_command_string() const;

        /* @return ( std::vector<char *> ): Get the full command but as C-style arguments for `execvp`
         * @description: Get the full command as a C-style arguments for sys calls
         */
        std::vector<char *> to_exec_args() const;

        // @return ( bool ): Check if the command is empty
        bool is_empty() const { return parts.empty(); }

        // @return ( std::string ): Get the command as a printable string wrapped in single quotes
        std::string get_print_string() const;
    };

    // Class to save configuration
    class Config
    {
      private:
        Config();
        Config(const Config &) = delete;
        Config &operator=(const Config &) = delete;

      public:
        /* INFO: Most of these options are just to save the configuration and wont be used by the library itself.
         * It is upto the user to use them or not and infact how to use them.
         * However, some of them are used by the library itself and are important.
         * 1. override_run: If set to true, it will disable the default run command and will not run the target executable.
         * 2. target_executable: Target executable to run. If not provided, it will run the target executable from config.
         * 3. cmd_args: It saves the command line arguments passed to the program.
         *
         * There are some params that can't be set in commnad line and are to be set in program itself.
         * 1. extra_args 2. use_extra_config_keys 3. the keys themself (you can only provide values in command line).
         * Make sure to set use_extra_config_keys before BLD_HANDLE_ARGS so the function knows whether to parse extra keys!
         */

        // if hot reload is enabled
        bool hot_reload;
        // if verbose output is enabled.
        bool verbose;
        // Override the run command.. If true, it will disable default run command ( runs target executable )
        bool override_run;
        // If user wants to execute their own commands, won't log error if somehting other than "config" or "run" is passed
        bool extra_args;
        // If user wants to add extra configuration keys.
        bool use_extra_config_keys;
        // Number of threads to use for parallel execution
        size_t threads;
        // Compiler command to use.
        std::string compiler;
        // Target executable to run
        std::string target_executable;
        // Target platform E.g win32, linux, darwin
        std::string target_platform;
        // Build directory
        std::string build_dir;
        // Compiler flags
        std::string compiler_flags;
        // Linker flags
        std::string linker_flags;
        // Command to run before build
        std::string pre_build_command;
        // Command to run after build
        std::string post_build_command;
        // Files to hot reload
        std::vector<std::string> hot_reload_files;
        // Save the command line arguments
        std::vector<std::string> cmd_args;
        // Extra configuration keys-value pairs
        std::unordered_map<std::string, std::string> extra_config_val;
        // Extra configuration keys-bool pairs
        std::unordered_map<std::string, bool> extra_config_bool;

        /* @brief: Get the singleton instance of the Config class
         * @description: This class works as a singleton and this function returns the instance of the class, this will be the only instance.
         */
        static Config &get();

        /* @brief: Initialize the configuration with default values
         * @description: sets the default values for the configuration
         *  compiler & target_platform if not provided
         */
        void init();

        /* @brief: Load the configuration from a file BLD_DEFAULT_CONFIG_FILE: "./build.conf")
         * @param filename ( std::string ): Name of the file to load the configuration from
         * @description: Load the configuration from a file. The file should be in the format of key=value pairs.
         */
        bool load_from_file(const std::string &filename);

        /* @brief: Save the configuration to a file BLD_DEFAULT_CONFIG_FILE: "./build.conf")
         * @param filename ( std::string ): Name of the file to save the configuration to.
         * @description: Save the configuration to a file. The file will be in the format of key=value pairs.
         */
        bool save_to_file(const std::string &filename);
    };

    /* @brief: Convert command-line arguments to vector of strings
   * @param argc ( int ): Number of arguments
   * @param argv ( char*[] ): Array of arguments
   */
    bool args_to_vec(int argc, char *argv[], std::vector<std::string> &args);

    /* @beief: Validate the command before executing
     * @param command ( Command ): Command to validate
     */
    bool validate_command(const Command &command);

    /* @brief: Wait for the process to complete
     * @param pid ( pid_t ): Process ID to wait for
     * @description: Wait for the process to complete and log the status. Use this function instead of direct waitpid
     */
    int wait_for_process(pid_t pid);

    /* @brief: Execute the command
       * @param command ( Command ): Command to execute, must be a valid process command and not shell command
       * @return: returns a code to indicate success or failure
       *   >1 : Command executed successfully, returns pid of fork.
       *    0 : Command failed to execute or something wrong on system side
       *   -1 : No command to execute or something wrong on user side
       * @description: Execute the command using fork and log the status alongwith
       */
    int execute(const Command &command);

    /* @brief: Execute the command asynchronously (without waiting)
     * @param command ( Command ): Command to execute, must be a valid process command and not shell command
     * @return: returns a code to indicate success or failure
     *   >0 : Command executed successfully, returns pid of fork.
     *    0 : Command failed to execute or something wrong on system side
     *   -1 : No command to execute or something wrong on user side
     * @description: Execute the command using fork and log the status alongwith
     */
    int execute_without_wait(const Command &command);

    // Return type for parallel execution
    struct Exec_par_result
    {
        size_t completed;                    // Number of successfully completed commands
        std::vector<size_t> failed_indices;  // Indices of commands that failed

        Exec_par_result() : completed(0) {}
    };

    /* @brief: Execute multiple commands on multiple threads.
     * @param cmds: Vector of commands to execute
     * @param threads: Number of parallel threads (default: hardware concurrency - 1). Change if you want.
     * @param strict: If true, stop all threads if an error occurs even in one command.
     * @return: Exec_par_result
     */
    bld::Exec_par_result execute_parallel(const std::vector<bld::Command> &cmds, size_t threads = (std::thread::hardware_concurrency() - 1),
                                          bool strict = true);

    /* @description: Print system metadata:
     *  1. Operating System
     *  2. Compiler
     *  3. Architecture
     */
    void print_metadata();

    /* @brief: Preprocess the command for shell execution
     * @param cmd ( Command ): Command to preprocess
     * @return: Preprocessed command for shell execution
     * @description: Preprocess the command for shell execution by adding shell command and arguments
     *    Windows:     cmd.exe /c <command>
     *    Linux/macOS: /bin/sh -c <command>
     */
    Command preprocess_commands_for_shell(const Command &cmd);

    /* @brief: Execute the shell command with preprocessed parts
     * param cmd ( Command ): Command to execute in shell
     * @description: Execute the shell command with preprocessed parts
     *    Uses execute function to execute the preprocessed command
     *  return the return value of the execute function
     */
    int execute_shell(std::string command);

    /* @brief: Execute the shell command with preprocessed parts but asks wether to execute or not first
     * @param cmd ( Command ): Command to execute in shell
     * @param prompt ( bool ): Ask for confirmation before executing
     * @description: Execute the shell command with preprocessed parts with prompting
     *    Uses execute function to execute the preprocessed command
     *    return the return value of the execute function
     */
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

    /* @brief: Check if the executable is outdated i.e. source file is newer than the executable
     * @param file_name ( std::string ): Source file name
     * @param executable ( std::string ): Executable file name
     * @description: Check if the source file is newer than the executable. Uses last write time to compare.
     *  Can be used for anything realistically, just enter paths and it will return.
     *  it basically returns ( modify_time(file) > modify_time(executable) ) irrespective of file type
     */
    bool is_executable_outdated(std::string file_name, std::string executable);

    /* @brief: Rebuild the executable if the source file is newer than the executable and runs it
     * @param filename ( std::string ): Source file name
     * @param executable ( std::string ): Executable file name
     * @param compiler ( std::string ): Compiler command to use (default: "")
     *  It can detect compiler itself if not provided
     *  Supported compilers: g++, clang++, cl
     *  @description: Generally used for actual build script
     */
    void rebuild_yourself_onchange_and_run(const std::string &filename, const std::string &executable, std::string compiler = "");

    /* @brief: Rebuild the executable if the source file is newer than the executable
     * @param filename ( std::string ): Source file name (C++ only)
     * @param executable ( std::string ): Executable file name
     * @param compiler ( std::string ): Compiler command to use (default: "")
     *  It can detect compiler itself if not provided
     *  Supported compilers: g++, clang++, cl
     *  @description: Actually for general use and can be used to rebuild any *C++* file since it doesn't restart the process
     */
    void rebuild_yourself_onchange(const std::string &filename, const std::string &executable, std::string compiler);

    /* @brief: Handle command-line arguments
     * @param argc ( int ): Number of arguments
     * @param argv ( char*[] ): Array of arguments
     * @description: Handle command-line arguments... Currently only handles 'run' and 'config' commands.
     *    run: Run the target executable or execute the command provided after run 'run <command>'
     *    config: Set the configuration for the build system and save it to a file. Saving is not handled by function but Config class.
     */
    void handle_args(int argc, char *argv[]);

    /* @brief: Handle the 'run' command
     * @param args ( std::vector<std::string> ): Arguments for the command
     * @description: Handle the 'run' command. If no arguments are provided, it runs the target executable from config.
     */
    int handle_run_command(std::vector<std::string> args);

    /* @brief: Handle the 'config' command
     * @param args ( std::vector<std::string> ): Arguments for the command
     * @description: Handle the 'config' command. Sets the configuration based on the arguments.
     */
    void handle_config_command(std::vector<std::string> args, std::string name);

    /* @brief: Check if a string starts with a prefix
     * @param str ( std::string ): String to check
     * @param prefix ( std::string ): Prefix to check
     * @description: If size of prefix is greater than size of string, returns false
     *  Uses std::string::compare() to compare the prefix with the string
     */
    bool starts_with(const std::string &str, const std::string &prefix);

    namespace fs
    {
        /* @brief: Read entire file content into a string
         * @param path: Path to the file
         * @param content: Reference to string where content will be stored
         * @return: True if successful, false otherwise
         */
        bool read_file(const std::string &path, std::string &content);

        /* @brief: Write string content to a file
         * @param path: Path to the file
         * @param content: Content to write
         * @return: true if successful, false otherwise
         */
        bool write_entire_file(const std::string &path, const std::string &content);

        /* @brief: Append string content to a file
         * @param path: Path to the file
         * @param content: Content to append
         * @return: true if successful, false otherwise
         */
        bool append_file(const std::string &path, const std::string &content);

        /* @brief: Read file line by line, calling a function for each line
         * @param path: Path to the file
         * @param func: Function to call for each line
         * @return: true if successful, false otherwise
         */
        bool read_lines(const std::string &path, std::vector<std::string> &lines);

        /* @brief: Replace text in file
         * @param path: Path to the file
         * @param from: Text to replace
         * @param to: Text to replace with
         * @return: true if successful, false otherwise
         */
        bool replace_in_file(const std::string &path, const std::string &from, const std::string &to);

        /* @brief: Copy a file
         * @param from: Source path
         * @param to: Destination path
         * @param overwrite = true: Whether to overwrite if destination exists
         * @return: true if successful, false otherwise
         */
        bool copy_file(const std::string &from, const std::string &to, bool overwrite = false);

        /* @brief: Move/Rename a file
         * @param from: Source path
         * @param to: Destination path
         * @return: true if successful, false otherwise
         */
        bool move_file(const std::string &from, const std::string &to);

        /* @brief: Get file extension
         * @param path: Path to the file
         * @return: File extension including the dot, or empty string if none
         */
        std::string get_extension(const std::string &path);

        /* @brief: Create directory and all parent directories if they don't exist
         * @param path: Path to create
         * @return: true if successful, false otherwise
         */
        bool create_directory(const std::string &path);

        /* @brief: Create directory and all parent directories if they don't exist
         * @param path: Path to create
         * @return: true if successful, false otherwise
         */
        bool create_dir_if_not_exists(const std::string &path);

        /* @brief: Remove directory and all its contents if it exists
         * @param path: Path to remove
         * @return: true if successful, false otherwise
         */
        bool remove_dir(const std::string &path);

        /* @brief: Get list of all files in directory
         * @param path: Directory path
         * @param recursive = true: Whether to include files in subdirectories
         * @return: Vector of file paths
         */
        std::vector<std::string> list_files_in_dir(const std::string &path, bool recursive = false);

        /* @brief: Get list of all directories in directory
         * @param path: Directory path
         * @param recursive = true: Whether to include subdirectories of subdirectories
         * @return: Vector of directory paths
         */
        std::vector<std::string> list_directories(const std::string &path, bool recursive = false);
    }  // namespace fs

    struct Dep
    {
        std::string target;                     // Target/output file
        std::vector<std::string> dependencies;  // Input files/dependencies
        bld::Command command;                   // Command to build the target
        bool is_phony{false};                   // Whether this is a phony target

        // Default constructor
        Dep() = default;

        // Constructor for non-phony targets
        Dep(std::string target, std::vector<std::string> dependencies = {}, bld::Command command = {});

        // Constructor for phony targets
        Dep(std::string target, std::vector<std::string> dependencies, bool is_phony);

        // Implicit conversion from initializer list for better usability
        Dep(std::string target, std::initializer_list<std::string> deps, bld::Command command = {});

        // Copy constructor
        Dep(const Dep &other);

        // Move constructor
        Dep(Dep &&other) noexcept;

        // Copy assignment
        Dep &operator=(const Dep &other);

        // Move assignment
        Dep &operator=(Dep &&other) noexcept;
    };

    class Dep_graph
    {
        struct Node
        {
            Dep dep;
            std::vector<std::string> dependencies;  // Names of dependent nodes
            bool visited{false};
            bool in_progress{false};  // For cycle detection
            bool checked{false};
            std::vector<std::string> waiting_on;  // files that need to be built before this one

            Node(const Dep &d) : dep(d) {}
        };
        std::unordered_map<std::string, std::unique_ptr<Node>> nodes;
        std::unordered_set<std::string> checked_sources;

      public:
        /* @brief Add a dependency to the graph.
         * @param dep The dependency to add.
         */
        void add_dep(const Dep &dep);

        /* @brief Add a phony target to the graph.
         * @param target The name of the phony target.
         * @param deps The dependencies of the phony target.
         */
        void add_phony(const std::string &target, const std::vector<std::string> &deps);

        /* @brief Check if a node needs to be rebuilt.
         * @param node The node to check.
         * @return true If the node needs to be rebuilt.
         * @return false If the node does not need to be rebuilt.
         */
        bool needs_rebuild(const Node *node);

        /* @brief Build the target.
         * @param target The name of the target to build.
         * @return true If the build was successful.
         * @return false If the build failed.
         */
        bool build(const std::string &target);

        /* @brief Build the dependency.
         * @param dep The dependency to build.
         * @return true If the build was successful.
         * @return false If the build failed.
         */
        bool build(const Dep &dep);

        /* @brief Build all targets in the graph.
         * @return true If all builds were successful.
         * @return false If any build failed.
         */
        bool build_all();

        /* @brief Build all targets in the graph but all previous checks are discarded (alternative method).
         * @return true If all builds were successful.
         * @return false If any build failed.
         */
        bool F_build_all();

        bool build_parallel(const std::string &target, size_t thread_count = std::thread::hardware_concurrency());
        bool build_all_parallel(size_t thread_count = std::thread::hardware_concurrency());

      private:
        /* @brief Build a node in the graph.
         * @param target The name of the target to build.
         * @return true If the build was successful.
         * @return false If the build failed.
         */
        bool build_node(const std::string &target);

        /* @brief Detect cycles in the graph.
         * @param target The name of the target to check.
         * @param visited The set of visited nodes.
         * @param in_progress The set of nodes currently being processed.
         * @return true If a cycle was detected.
         * @return false If no cycle was detected.
         */
        bool detect_cycle(const std::string &target, std::unordered_set<std::string> &visited,
                          std::unordered_set<std::string> &in_progress);
        /* @brief Prepare graph for parallel build
         * @param target The target to build
         * @param ready_targets The queue of targets
         */
        bool prepare_build_graph(const std::string &target, std::queue<std::string> &ready_targets);

        /* @brief Process completed target
         * @param target The target that was completed
         * @param ready_targets The queue of targets
         * @param queue_mutex The mutex for the queue
         * @param cv The condition variable
         */
        void process_completed_target(const std::string &target, std::queue<std::string> &ready_targets, std::mutex &queue_mutex,
                                      std::condition_variable &cv);
    };

}  // namespace bld

#ifdef B_LDR_IMPLEMENTATION

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>
#include <queue>
#include <sstream>

void bld::log(bld::Log_type type, const std::string &msg)
{
    switch (type)
    {
        case Log_type::INFO:
            std::cout << "[INFO]: " << msg << std::endl;
            break;
        case Log_type::WARNING:
            std::cout << "[WARNING]: " << msg << std::endl;
            break;
        case Log_type::ERROR:
            std::cerr << "[ERROR]: " << msg << std::flush << std::endl;
            break;
        case Log_type::DEBUG:
            std::cout << "[DEBUG]: " << msg << std::flush << std::endl;
            break;
        default:
            std::cout << "[UNKNOWN]: " << msg << std::endl;
            break;
    }
}

// Get the full command as a single string
std::string bld::Command::get_command_string() const
{
    std::stringstream ss;
    for (const auto &part : parts)
        ss << part << " ";
    return ss.str();
}

// Convert parts to C-style arguments for `execvp`
std::vector<char *> bld::Command::to_exec_args() const
{
    std::vector<char *> exec_args;
    for (const auto &part : parts)
        exec_args.push_back(const_cast<char *>(part.c_str()));
    exec_args.push_back(nullptr);  // Null terminator
    return exec_args;
}

std::string bld::Command::get_print_string() const
{
    if (parts.empty())
        return "''";
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
bld::Command::Command(Args... args)
{
    (parts.emplace_back(args), ...);
}

template <typename... Args>
void bld::Command::add_parts(Args... args)
{
    (parts.emplace_back(args), ...);
}

bool bld::validate_command(const bld::Command &command)
{
    bld::log(bld::Log_type::WARNING, "Do you want to execute " + command.get_print_string() + "in shell");
    std::cerr << "  [WARNING]: Answer[y/n]: ";
    std::string response;
    std::getline(std::cin, response);
    return (response == "y" || response == "Y");
}

int bld::wait_for_process(pid_t pid)
{
    int status;
    waitpid(pid, &status, 0);  // Wait for the process to complete

    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0)
        {
            bld::log(bld::Log_type::ERROR, "Process exited with non-zero status: " + std::to_string(exit_code));
            return -1;  // Return exit code for failure
        }
        bld::log(bld::Log_type::INFO, "Process exited successfully.");
    }
    else if (WIFSIGNALED(status))
    {
        int signal = WTERMSIG(status);
        bld::log(bld::Log_type::ERROR, "Process terminated by signal: " + std::to_string(signal));
        return -1;  // Indicate signal termination
    }
    else
    {
        bld::log(bld::Log_type::WARNING, "Unexpected process termination status.");
    }

    return static_cast<int>(pid);  // Indicate success
}

int bld::execute(const Command &command)
{
    if (command.is_empty())
    {
        bld::log(Log_type::ERROR, "No command to execute.");
        return -1;
    }

    auto args = command.to_exec_args();
    bld::log(Log_type::INFO, "Executing command: " + command.get_print_string());

    pid_t pid = fork();
    if (pid == -1)
    {
        bld::log(Log_type::ERROR, "Failed to create child process.");
        return 0;
    }
    else if (pid == 0)
    {
        // Child process
        if (execvp(args[0], args.data()) == -1)
        {
            perror("execvp");
            bld::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }
        // This line should never be reached
        bld::log(Log_type::ERROR, "Unexpected code execution after execvp. Did we find a bug? in libc or kernel?");
        abort();
    }

    return bld::wait_for_process(pid);  // Use wait_for_process instead of direct waitpid
}

int bld::execute_without_wait(const Command &command)
{
    if (command.is_empty())
    {
        bld::log(Log_type::ERROR, "No command to execute.");
        return -1;
    }

    auto args = command.to_exec_args();
    bld::log(Log_type::INFO, "Executing command: " + command.get_print_string());

    pid_t pid = fork();
    if (pid == -1)
    {
        bld::log(Log_type::ERROR, "Failed to create child process.");
        return 0;
    }
    else if (pid == 0)
    {
        // Child process
        if (execvp(args[0], args.data()) == -1)
        {
            perror("execvp");
            bld::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }
        // This line should never be reached
        bld::log(Log_type::ERROR, "Unexpected code execution after execvp. Did we find a bug? in libc or kernel?");
        abort();
    }

    return static_cast<int>(pid);  // Use wait_for_process instead of direct waitpid
}

bld::Exec_par_result bld::execute_parallel(const std::vector<bld::Command> &cmds, size_t threads, bool strict)
{
    bld::Exec_par_result result;

    if (cmds.empty())
        return result;  // Nothing to do
    if (threads == 0)
        threads = 1;
    if (threads > std::thread::hardware_concurrency())
        threads = std::thread::hardware_concurrency();

    std::mutex queue_mutex, output_mutex;
    std::atomic<bool> stop_workers{false};  // Used when strict = true

    // Queue of command indices to process
    std::queue<size_t> cmd_queue;
    for (size_t i = 0; i < cmds.size(); ++i)
        cmd_queue.push(i);

    bld::log(bld::Log_type::INFO, "Executing " + std::to_string(cmds.size()) + " commands on " + std::to_string(threads) + " threads...");

    // Worker function
    auto worker = [&]()
    {
        while (true)
        {
            if (strict && stop_workers)
                return;  // Stop all threads if strict and an error occurs

            size_t cmd_idx;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (cmd_queue.empty())
                    return;

                cmd_idx = cmd_queue.front();
                cmd_queue.pop();
            }

            // Log and execute the command
            {
                std::lock_guard<std::mutex> lock(output_mutex);
            }

            int execution_result = execute(cmds[cmd_idx]);

            if (execution_result <= 0)
            {
                {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    log(Log_type::ERROR, "Failed to execute: " + cmds[cmd_idx].get_print_string());
                }

                // Record the failed command index
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    result.failed_indices.push_back(cmd_idx);
                }

                if (strict)
                {
                    stop_workers = true;  // Signal all threads to stop
                    return;
                }
            }
            else
            {
                // Increment completed count
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    bld::log(bld::Log_type::INFO, "Completed: " + cmds[cmd_idx].get_print_string());
                    result.completed++;
                }
            }
        }
    };

    // Launch worker threads
    std::vector<std::thread> workers;
    size_t num_threads = std::min(threads, cmds.size());

    for (size_t i = 0; i < num_threads; ++i)
        workers.emplace_back(worker);

    // Wait for all threads to complete
    for (auto &t : workers)
        if (t.joinable())
            t.join();

    return result;
}

void bld::print_metadata()
{
    std::cout << '\n';
    log(Log_type::INFO, "Printing system metadata...........................................");
    // 1. Get OS information
    struct utsname sys_info;
    if (uname(&sys_info) == 0)
    {
        std::cout << "    Operating System: " << sys_info.sysname << " " << sys_info.release << " (" << sys_info.machine << ")"
                  << std::endl;
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

bld::Command bld::preprocess_commands_for_shell(const bld::Command &cmd)
{
    bld::Command cmd_s;

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
int bld::execute_shell(std::string cmd)
{
    // Preprocess the command for shell execution
    auto cmd_s = preprocess_commands_for_shell(cmd);

    // Execute the shell command using the original execute function
    return execute(cmd_s);
}

int bld::execute_shell(std::string cmd, bool prompt)
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

bool bld::read_process_output(const Command &cmd, std::string &output, size_t buffer_size)
{
    if (cmd.is_empty())
    {
        bld::log(Log_type::ERROR, "No command to execute.");
        return false;
    }

    if (buffer_size == 0)
    {
        bld::log(Log_type::ERROR, "Buffer size cannot be zero.");
        return false;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        bld::log(Log_type::ERROR, "Failed to create pipe: " + std::string(strerror(errno)));
        return false;
    }

    auto args = cmd.to_exec_args();
    bld::log(Log_type::INFO, "Extracting output from: " + cmd.get_print_string());

    pid_t pid = fork();
    if (pid == -1)
    {
        bld::log(Log_type::ERROR, "Failed to create child process: " + std::string(strerror(errno)));
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
            bld::log(Log_type::ERROR, "Failed with error: " + std::string(strerror(errno)));
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

        return bld::wait_for_process(pid) > 0;  // Use wait_for_process to handle the exit status
    }
}

bool bld::read_shell_output(const std::string &cmd, std::string &output, size_t buffer_size)
{
    if (buffer_size == 0)
    {
        bld::log(Log_type::ERROR, "Buffer size cannot be zero.");
        return false;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        bld::log(Log_type::ERROR, "Failed to create pipe: " + std::string(strerror(errno)));
        return false;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        bld::log(Log_type::ERROR, "Failed to create child process: " + std::string(strerror(errno)));
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

bool bld::is_executable_outdated(std::string file_name, std::string executable)
{
    try
    {
        // Check if the source file exists
        if (!std::filesystem::exists(file_name))
        {
            bld::log(Log_type::ERROR, "Source file does not exist: " + file_name);
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
        bld::log(Log_type::ERROR, "Filesystem error: " + std::string(e.what()));
        return false;  // Or handle the error differently
    }
    catch (const std::exception &e)
    {
        bld::log(Log_type::ERROR, std::string(e.what()));
        return false;  // Or handle the error differently
    }
}

void bld::rebuild_yourself_onchange_and_run(const std::string &filename, const std::string &executable, std::string compiler)
{
    namespace fs = std::filesystem;
    // Convert to filesystem paths
    fs::path source_path(filename);
    fs::path exec_path(executable);
    fs::path backup_path = exec_path.string() + ".old";

    if (!bld::is_executable_outdated(filename, executable))
        return;  // No rebuild needed

    bld::log(Log_type::INFO, "Build executable not up-to-date. Rebuilding...");

    // Create backup of existing executable if it exists
    if (fs::exists(exec_path))
    {
        try
        {
            if (fs::exists(backup_path))
                fs::remove(backup_path);  // Remove existing backup
            fs::rename(exec_path, backup_path);
            bld::log(Log_type::INFO, "Created backup at: " + backup_path.string());
        }
        catch (const fs::filesystem_error &e)
        {
            bld::log(Log_type::ERROR, "Failed to create backup: " + std::string(e.what()));
            return;
        }
    }

    // Detect the compiler if not provided
    if (compiler.empty())
    {
#ifdef __clang__
        compiler = "clang++";
#elif defined(__GNUC__)
        compiler = "g++";
#elif defined(_MSC_VER)
        compiler = "cl";  // MSVC
#else
        bld::log(Log_type::ERROR, "Unknown compiler. Defaulting to g++.");
        compiler = "g++";
#endif
    }

    // Set up the compile command
    bld::Command cmd;
    cmd.parts = {compiler, source_path.string(), "-o", exec_path.string()};

    // Execute the compile command
    int compile_result = bld::execute(cmd);
    if (compile_result <= 0)
    {
        bld::log(Log_type::ERROR, "Compilation failed.");

        // Restore backup if compilation failed and backup exists
        if (fs::exists(backup_path))
        {
            try
            {
                fs::remove(exec_path);  // Remove failed compilation output if it exists
                fs::rename(backup_path, exec_path);
                bld::log(Log_type::INFO, "Restored previous executable from backup.");
            }
            catch (const fs::filesystem_error &e)
            {
                bld::log(Log_type::ERROR, "Failed to restore backup: " + std::string(e.what()));
            }
        }
        return;
    }

    bld::log(Log_type::INFO, "Compilation successful. Restarting w/o any args for safety...");

    // Verify the new executable exists and is executable
    if (!fs::exists(exec_path))
    {
        bld::log(Log_type::ERROR, "New executable not found after successful compilation.");
        return;
    }

    // Make sure the new executable has proper permissions
    try
    {
        fs::permissions(exec_path, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);
    }
    catch (const fs::filesystem_error &e)
    {
        bld::log(Log_type::WARNING, "Failed to set executable permissions: " + std::string(e.what()));
    }

    // Run the new executable
    bld::Command restart_cmd;
    restart_cmd.parts = {exec_path.string()};

    int restart_result = bld::execute(restart_cmd);
    if (restart_result <= 0)
    {
        bld::log(Log_type::ERROR, "Failed to start new executable.");
        return;
    }

    // Only remove backup after successful restart
    try
    {
        if (fs::exists(backup_path))
            fs::remove(backup_path);
    }
    catch (const fs::filesystem_error &e)
    {
        bld::log(Log_type::WARNING, "Failed to remove backup: " + std::string(e.what()));
    }

    // Exit the current process after successfully restarting
    std::exit(EXIT_SUCCESS);
}

void bld::rebuild_yourself_onchange(const std::string &filename, const std::string &executable, std::string compiler)
{
    if (bld::is_executable_outdated(filename, executable))
    {
        bld::log(Log_type::INFO, "Build executable not up-to-date. Rebuilding...");
        bld::Command cmd = {};

        // Detect the compiler if not provided
        if (compiler.empty())
        {
#ifdef __clang__
            compiler = "clang++";
#elif defined(__GNUC__)
            compiler = "g++";
#elif defined(_MSC_VER)
            compiler = "cl";  // MSVC uses 'cl' as the compiler command
#else
            bld::log(Log_type::ERROR, "Unknown compiler. Defaulting to g++.");
            compiler = "g++";
#endif
        }

        // Set up the compile command
        cmd.parts = {compiler, filename, "-o", executable};

        // Execute the compile command
        if (bld::execute(cmd) <= 0)
        {
            bld::log(Log_type::WARNING, "Failed to rebuild executable.");
            return;
        }
    }
}

bool bld::args_to_vec(int argc, char *argv[], std::vector<std::string> &args)
{
    if (argc < 1)
        return false;

    args.reserve(argc - 1);
    for (int i = 1; i < argc; i++)
        args.push_back(argv[i]);

    return true;
}

bld::Config::Config() : hot_reload_files(), cmd_args()
{
    hot_reload = false;
    extra_args = false;
    verbose = false;
    override_run = false;
    use_extra_config_keys = false;
    threads = 1;
    compiler = "";
    target_executable = "";
    target_platform = "";
    build_dir = "build";
    compiler_flags = "";
    linker_flags = "";
    pre_build_command = "";
    post_build_command = "";

    init();
    // Automatically load configuration if the file exists
    if (std::filesystem::exists(BLD_DEFAULT_CONFIG_FILE))
        load_from_file(BLD_DEFAULT_CONFIG_FILE);
}

bld::Config &bld::Config::get()
{
#ifdef BLD_USE_CONFIG
    static Config instance;
    return instance;
#else
    bld::log(bld::Log_type::ERROR, "Config is disabled. Please enable BLD_USE_CONFIG macro to use the Config class.");
    exit(1);
#endif  // BLD_USE_CONFIG
}

void bld::Config::init()
{
#ifdef _WIN32
    target_platform = "win32";
#elif defined(__APPLE__)
    target_platform = "darwin";
#elif defined(__linux__)
    target_platform = "linux";
#else
    target_platform = "unknown";
#endif

#ifdef __clang__
    compiler = "clang++";
#elif defined(__GNUC__)
    compiler = "g++";
#elif defined(_MSC_VER)
    compiler = "cl";
#else
    compiler = "g++";
#endif
}

bool bld::Config::load_from_file(const std::string &filename)
{
    if (!std::filesystem::exists(filename))
    {
        bld::log(bld::Log_type::WARNING, "Config file not found: " + filename);
        return false;
    }

    std::ifstream file(filename);
    if (!file.is_open())
    {
        bld::log(bld::Log_type::ERROR, "Failed to open config file: " + filename);
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::stringstream ss(line);
        std::string key, value;
        std::getline(ss, key, '=');
        std::getline(ss, value);

        if (key == "hot_reload")
            hot_reload = (value == "true");
        else if (key == "compiler")
            compiler = value;
        else if (key == "target")
            target_executable = value;
        else if (key == "platform")
            target_platform = value;
        else if (key == "build_dir")
            build_dir = value;
        else if (key == "compiler_flags")
            compiler_flags = value;
        else if (key == "linker_flags")
            linker_flags = value;
        else if (key == "verbose")
            verbose = (value == "true");
        else if (key == "pre_build_command")
            pre_build_command = value;
        else if (key == "post_build_command")
            post_build_command = value;
        else if (key == "override_run")
            override_run = (value == "true");
        else if (key == "hot_reload_files")
        {
            hot_reload_files.clear();
            std::stringstream fs(value);
            std::string file;
            while (std::getline(fs, file, ','))
                hot_reload_files.push_back(file);
        }
        else
        {
            bld::log(bld::Log_type::WARNING, "Unknown key in config file: " + key);
        }
    }
    return true;
}

bool bld::Config::save_to_file(const std::string &filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
        return false;

    if (hot_reload)
        file << "hot_reload=true\n";
    if (!compiler.empty())
        file << "compiler=" << compiler << "\n";
    if (!target_executable.empty())
        file << "target=" << target_executable << "\n";
    if (!target_platform.empty())
        file << "platform=" << target_platform << "\n";
    if (!build_dir.empty())
        file << "build_dir=" << build_dir << "\n";
    if (!compiler_flags.empty())
        file << "compiler_flags=" << compiler_flags << "\n";
    if (!linker_flags.empty())
        file << "linker_flags=" << linker_flags << "\n";
    if (verbose)
        file << "verbose=true\n";
    if (!pre_build_command.empty())
        file << "pre_build_command=" << pre_build_command << "\n";
    if (!post_build_command.empty())
        file << "post_build_command=" << post_build_command << "\n";
    if (override_run)
        file << "override_run=true\n";

    if (!hot_reload_files.empty())
    {
        file << "files=";
        for (size_t i = 0; i < hot_reload_files.size(); ++i)
        {
            file << hot_reload_files[i];
            if (i < hot_reload_files.size() - 1)
                file << ",";
        }
        file << "\n";
    }

    return true;
}

int bld::handle_run_command(std::vector<std::string> args)
{
#ifdef BLD_USE_CONFIG
    if (args.size() == 2)
    {
        bld::log(bld::Log_type::WARNING, "Command 'run' specified with the executable");
        bld::log(bld::Log_type::INFO, "Proceeding to run the specified command: " + args[1]);
        bld::Command cmd(args[1]);
        return bld::execute(cmd);
    }
    else if (args.size() > 2)
    {
        bld::log(bld::Log_type::ERROR, "Too many arguments for 'run' command. Only executables are supported.");
        bld::log(bld::Log_type::INFO, "Usage: run <executable>");
        exit(EXIT_FAILURE);
    }
    if (bld::Config::get().target_executable.empty())
    {
        bld::log(bld::Log_type::ERROR, "No target executable specified in config");
        exit(1);
    }

    bld::Command cmd;
    cmd.parts.push_back(Config::get().target_executable);

    bld::execute(cmd);
    exit(EXIT_SUCCESS);
#else
    if (args.size() < 2)
    {
        bld::log(
            bld::Log_type::ERROR,
            "No target executable specified in config. Config is disabled. Please enable BLD_USE_CONFIG macro to use the Config class.");
        exit(EXIT_FAILURE);
    }
    else if (args.size() == 2)
    {
        bld::log(bld::Log_type::WARNING, "Command 'run' specified with the executable");
        bld::log(bld::Log_type::INFO, "Proceeding to run the specified command: " + args[1]);
        bld::Command cmd(args[1]);
        return bld::execute(cmd);
    }
    else if (args.size() > 2)
    {
        bld::log(bld::Log_type::ERROR, "Too many arguments for 'run' command. Only executables are supported.");
        bld::log(bld::Log_type::INFO, "Usage: run <executable>");
        exit(EXIT_FAILURE);
    }
#endif
    bld::log(bld::Log_type::ERROR, "Should never be reached: " + std::to_string(__LINE__));
    exit(EXIT_FAILURE);
}

bool bld::starts_with(const std::string &str, const std::string &prefix)
{
    if (prefix.size() > str.size())
        return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

void bld::handle_config_command(std::vector<std::string> args, std::string name)
{
    if (args.size() < 2)
    {
        log(bld::Log_type::ERROR, "Config command requires arguments");
        std::string usage = name + " config -[key]=value \n" + "        E.g: ' " + name + " config -verbose=true '";
        log(bld::Log_type::INFO, "Usage: " + usage);
        return;
    }

    auto &config = bld::Config::get();

    for (size_t i = 1; i < args.size(); i++)
    {
        const auto &arg = args[i];
        if (bld::starts_with(arg, "-hreload="))
            config.hot_reload = (arg.substr(9) == "true");
        else if (bld::starts_with(arg, "-hreload"))
            config.hot_reload = true;
        else if (bld::starts_with(arg, "-threads="))
        {
            std::string number = arg.substr(9);
            if (number.empty())
            {
                bld::log(bld::Log_type::WARNING, "No value provided for threads. Setting 1.");
                config.threads = 1;
                continue;
            }
            else if (number.find_first_not_of("0123456789") != std::string::npos)
            {
                bld::log(bld::Log_type::ERROR, "Invalid value for threads: " + number);
                continue;
            }
            config.threads = std::stoi(number);
        }
        else if (bld::starts_with(arg, "-j="))
        {
            std::string number = arg.substr(3);
            if (number.empty())
            {
                bld::log(bld::Log_type::WARNING, "No value provided for threads. Setting 1.");
                config.threads = 1;
                continue;
            }
            else if (number.find_first_not_of("0123456789") != std::string::npos)
            {
                bld::log(bld::Log_type::ERROR, "Invalid value for threads: " + number);
                continue;
            }
            config.threads = std::stoi(number);
        }
        else if (bld::starts_with(arg, "-compiler="))
            config.compiler = arg.substr(10);
        else if (bld::starts_with(arg, "-target="))
            config.target_executable = arg.substr(8);
        else if (bld::starts_with(arg, "-build_dir="))
            config.build_dir = arg.substr(11);
        else if (bld::starts_with(arg, "-compiler_flags="))
            config.compiler_flags = arg.substr(16);
        else if (bld::starts_with(arg, "-linker_flags="))
            config.linker_flags = arg.substr(14);
        else if (bld::starts_with(arg, "-verbose="))
            config.verbose = (arg.substr(9) == "true");
        else if (bld::starts_with(arg, "-v") && arg.size() == 2)
            config.verbose = true;
        else if (bld::starts_with(arg, "-pre_build_command="))
            config.pre_build_command = arg.substr(19);
        else if (bld::starts_with(arg, "-post_build_command="))
            config.post_build_command = arg.substr(20);
        else if (bld::starts_with(arg, "-override_run="))
            config.override_run = (arg.substr(14) == "true");
        else if (bld::starts_with(arg, "-hr_files="))
        {
            config.hot_reload_files.clear();
            std::stringstream fs(arg.substr(7));
            std::string file;
            while (std::getline(fs, file, ','))
                config.hot_reload_files.push_back(file);
        }
        else if (bld::starts_with(arg, "-hr_files_app="))
        {
            std::stringstream fs(arg.substr(7));
            std::string file;
            while (std::getline(fs, file, ','))
                if (std::find(config.hot_reload_files.begin(), config.hot_reload_files.end(), file) == config.hot_reload_files.end())
                    config.hot_reload_files.push_back(file);
                else
                    bld::log(bld::Log_type::WARNING, "File already exists in hot reload list: " + file);
        }
        else if (bld::starts_with(arg, "-hr_files_rem="))
        {
            std::stringstream fs(arg.substr(7));
            std::string file;
            while (std::getline(fs, file, ','))
            {
                auto it = std::find(config.hot_reload_files.begin(), config.hot_reload_files.end(), file);
                if (it != config.hot_reload_files.end())
                    config.hot_reload_files.erase(it);
                else
                    bld::log(bld::Log_type::WARNING, "File not found in hot reload list: " + file);
            }
        }
        else if (bld::starts_with(arg, "-") && config.use_extra_config_keys)
        {
            std::string key = arg.substr(1, arg.find('=') - 1);
            std::string value = arg.substr(arg.find('=') + 1);
            if (key.empty())
            {
                bld::log(bld::Log_type::WARNING, "Key not provided: " + arg + ". No value will be set!");
                continue;
            }
            else if (value.empty())
            {
                bld::log(bld::Log_type::WARNING, "Value not provided: " + arg + ". No value will be set!");
                continue;
            }
            else if (value == "true" || value == "false")
            {
                if (config.extra_config_bool.find(key) != config.extra_config_bool.end())
                    config.extra_config_bool[key] = (value == "true");
                else
                    bld::log(bld::Log_type::WARNING, "Unknown key: " + key + ". No value will be set.");
                continue;
            }
            else
            {
                if (config.extra_config_val.find(key) != config.extra_config_val.end())
                    config.extra_config_val[key] = (value == "true");
                else
                    bld::log(bld::Log_type::WARNING, "Unknown key: " + key + ". No value will be set.");
                continue;
            }
        }
        else
        {
            bld::log(bld::Log_type::ERROR, "Unknown argument for config: ' " + arg + " '. Remember to use the format '-key=value'");
            bld::log(bld::Log_type::INFO,
                     "If ' " + arg + " ' this is a valid key for config, consider configuring Config before 'BLD_HANDLE_ARGS' macro.");
        }
    }

    // Save the updated configuration to file
    config.save_to_file(BLD_DEFAULT_CONFIG_FILE);
    bld::log(bld::Log_type::INFO, "Configuration saved to: " + std::string(BLD_DEFAULT_CONFIG_FILE));
}

void bld::handle_args(int argc, char *argv[])
{
    std::vector<std::string> args;
    if (bld::args_to_vec(argc, argv, args))
    {
        bld::Config::get().cmd_args = args;
        if (args.size() <= 0)
            return;
        else
        {
            std::string command = args[0];
            if (command == "run")
            {
#ifdef BLD_USE_CONFIG
                if (!bld::Config::get().override_run)
                    bld::handle_run_command(args);
#else
                bld::handle_run_command(args);
#endif
            }
            else if (command == "config")
            {
#ifdef BLD_USE_CONFIG
                bld::handle_config_command(args, argv[0]);
#else
                bld::log(bld::Log_type::ERROR, "Config is disabled. Please enable BLD_USE_CONFIG macro to use the Config class.");
#endif  // BLD_USE_CONFIG
            }
        }
    }
}

bool bld::fs::read_file(const std::string &path, std::string &content)
{
    if (!std::filesystem::exists(path))
    {
        bld::log(bld::Log_type::ERROR, "File does not exist: " + path);
        return false;
    }

    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        bld::log(bld::Log_type::ERROR, "Failed to open file: " + path);
        return false;
    }

    content = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    file.close();
    return true;
}

bool bld::fs::write_entire_file(const std::string &path, const std::string &content)
{
    std::ofstream file(path, std::ios::binary);

    if (!file)
    {
        bld::log(bld::Log_type::ERROR, "Failed to open file for writing: " + path);
        return false;
    }

    file << content;
    bool success = file.good();
    file.close();
    return success;
}

bool bld::fs::append_file(const std::string &path, const std::string &content)
{
    std::ofstream file(path, std::ios::app | std::ios::binary);
    if (!file)
    {
        bld::log(bld::Log_type::ERROR, "Failed to open file for appending: " + path);
        return false;
    }

    file << content;
    bool success = file.good();
    file.close();
    return success;
}

bool bld::fs::read_lines(const std::string &path, std::vector<std::string> &lines)
{
    std::ifstream file(path);
    if (!file)
    {
        bld::log(bld::Log_type::ERROR, "Failed to open file: " + path);
        return false;
    }

    std::string line;
    while (std::getline(file, line))
        lines.push_back(line);

    return true;
}

bool bld::fs::replace_in_file(const std::string &path, const std::string &from, const std::string &to)
{
    std::string content = "";
    if (!bld::fs::read_file(path, content))
    {
        bld::log(bld::Log_type::ERROR, "Failed to read file: " + path);
        return false;
    }
    if (content.empty())
    {
        bld::log(bld::Log_type::ERROR, "Failed to read file or it is empty: " + path);
        return false;
    }
    size_t pos = 0;
    while ((pos = content.find(from, pos)) != std::string::npos)
    {
        content.replace(pos, from.length(), to);
        pos += to.length();
    }

    return bld::fs::write_entire_file(path, content);
}

bool bld::fs::copy_file(const std::string &from, const std::string &to, bool overwrite)
{
    try
    {
        if (!overwrite && std::filesystem::exists(to))
        {
            bld::log(bld::Log_type::ERROR, "Destination file already exists: " + to);
            return false;
        }
        std::filesystem::copy_file(from, to,
                                   overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none);
        return true;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to copy file: " + std::string(e.what()));
        return false;
    }
}

bool bld::fs::move_file(const std::string &from, const std::string &to)
{
    try
    {
        std::filesystem::rename(from, to);
        return true;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to move file: " + std::string(e.what()));
        return false;
    }
}

std::string bld::fs::get_extension(const std::string &path)
{
    if (!std::filesystem::exists(path))
    {
        bld::log(bld::Log_type::ERROR, "File for extension request does not exist: " + path);
        return "";
    }
    std::filesystem::path p(path);
    return p.extension().string();
}

bool bld::fs::create_directory(const std::string &path)
{
    try
    {
        return std::filesystem::create_directories(path);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool bld::fs::create_dir_if_not_exists(const std::string &path)
{
    if (std::filesystem::exists(path))
    {
        bld::log(bld::Log_type::INFO, "Directory to create already exists: " + path);
        return true;
    }

    try
    {
        bool created = std::filesystem::create_directories(path);
        if (created)
            bld::log(bld::Log_type::INFO, "Directory created: " + path);
        else
            bld::log(bld::Log_type::ERROR, "Failed to create directory: " + path);
        return created;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool bld::fs::remove_dir(const std::string &path)
{
    if (!std::filesystem::exists(path))
    {
        bld::log(bld::Log_type::INFO, "Directory does not exist: " + path);
        return true;
    }

    try
    {
        std::uintmax_t removed_count = std::filesystem::remove_all(path);
        if (removed_count > 0)
            bld::log(bld::Log_type::INFO, "Directory removed: " + path);
        else
            bld::log(bld::Log_type::ERROR, "Failed to remove directory: " + path);
        return removed_count > 0;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to remove directory: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> bld::fs::list_files_in_dir(const std::string &path, bool recursive)
{
    std::vector<std::string> files;
    try
    {
        if (recursive)
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path))
                if (entry.is_regular_file())
                    files.push_back(entry.path().string());
        }
        else
        {
            for (const auto &entry : std::filesystem::directory_iterator(path))
                if (entry.is_regular_file())
                    files.push_back(entry.path().string());
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to list files: " + std::string(e.what()));
    }
    return files;
}

std::vector<std::string> bld::fs::list_directories(const std::string &path, bool recursive)
{
    std::vector<std::string> directories;
    try
    {
        if (recursive)
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path))
                if (entry.is_directory())
                    directories.push_back(entry.path().string());
        }
        else
        {
            for (const auto &entry : std::filesystem::directory_iterator(path))
                if (entry.is_directory())
                    directories.push_back(entry.path().string());
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        bld::log(bld::Log_type::ERROR, "Failed to list directories: " + std::string(e.what()));
    }
    return directories;
}

bld::Dep::Dep(std::string target, std::vector<std::string> dependencies, bld::Command command)
    : target(std::move(target)), dependencies(std::move(dependencies)), command(std::move(command))
{
}

// Constructor for phony targets
bld::Dep::Dep(std::string target, std::vector<std::string> dependencies, bool is_phony)
    : target(std::move(target)), dependencies(std::move(dependencies)), is_phony(is_phony)
{
}

// Implicit conversion from initializer list for better usability
bld::Dep::Dep(std::string target, std::initializer_list<std::string> deps, bld::Command command)
    : target(std::move(target)), dependencies(deps), command(std::move(command))
{
}

// Copy constructor
bld::Dep::Dep(const Dep &other) : target(other.target), dependencies(other.dependencies), command(other.command), is_phony(other.is_phony)
{
}

// Move constructor
bld::Dep::Dep(Dep &&other) noexcept
    : target(std::move(other.target)),
      dependencies(std::move(other.dependencies)),
      command(std::move(other.command)),
      is_phony(other.is_phony)
{
}

// Copy assignment
bld::Dep &bld::Dep::operator=(const Dep &other)
{
    if (this != &other)
    {
        target = other.target;
        dependencies = other.dependencies;
        command = other.command;
        is_phony = other.is_phony;
    }
    return *this;
}

// Move assignment
bld::Dep &bld::Dep::operator=(Dep &&other) noexcept
{
    if (this != &other)
    {
        target = std::move(other.target);
        dependencies = std::move(other.dependencies);
        command = std::move(other.command);
        is_phony = other.is_phony;
    }
    return *this;
}

void bld::Dep_graph::add_dep(const bld::Dep &dep)
{
    // We create a node and save it to map with key "File"
    auto node = std::make_unique<Node>(dep);
    node->dependencies = dep.dependencies;
    nodes[dep.target] = std::move(node);
}

void bld::Dep_graph::add_phony(const std::string &target, const std::vector<std::string> &deps)
{
    Dep phony_dep;
    phony_dep.target = target;
    phony_dep.dependencies = deps;
    phony_dep.is_phony = true;
    add_dep(phony_dep);
}

bool bld::Dep_graph::needs_rebuild(const Node *node)
{
    if (node->dep.is_phony)
        return true;
    if (!std::filesystem::exists(node->dep.target))
        return !node->dep.dependencies.empty();

    auto target_time = std::filesystem::last_write_time(node->dep.target);
    for (const auto &dep : node->dep.dependencies)
    {
        if (!std::filesystem::exists(dep))
        {
            bld::log(bld::Log_type::ERROR, "Dependency does not exist: " + dep);
            return true;  // Missing dependency forces rebuild
        }
        if (std::filesystem::last_write_time(dep) > target_time)
            return true;
    }
    return false;  // Explicit return when no rebuild needed
}

bool bld::Dep_graph::build(const std::string &target)
{
    std::unordered_set<std::string> visited, in_progress;
    if (detect_cycle(target, visited, in_progress))
    {
        bld::log(bld::Log_type::ERROR, "Circular dependency detected for target: " + target);
        return false;
    }
    checked_sources.clear();
    return build_node(target);
}

bool bld::Dep_graph::build(const Dep &dep)
{
    add_dep(dep);
    return build(dep.target);
}

bool bld::Dep_graph::build_all()
{
    bool success = true;
    for (const auto &node : nodes)
        if (!build(node.first))
            success = false;
    return success;
}

bool bld::Dep_graph::F_build_all()
{
    checked_sources.clear();
    bool success = true;
    for (const auto &node : nodes)
        if (!build(node.first))
            success = false;
    return success;
}

bool bld::Dep_graph::build_node(const std::string &target)
{
    auto it = nodes.find(target);
    if (it == nodes.end())
    {
        if (std::filesystem::exists(target))
        {
            if (checked_sources.find(target) == checked_sources.end())
            {
                bld::log(bld::Log_type::INFO, "Using existing source file: " + target);
                checked_sources.insert(target);
            }
            return true;
        }
        bld::log(bld::Log_type::ERROR, "Target not found: " + target);
        return false;
    }

    Node *node = it->second.get();
    if (node->checked)  // Skip if we've already checked this node
        return true;

    // First build all dependencies
    for (const auto &dep : node->dependencies)
        if (!build_node(dep))
            return false;

    // Check if we need to rebuild
    if (!needs_rebuild(node))
    {
        bld::log(bld::Log_type::INFO, "Target up to date: " + target);
        node->checked = true;
        return true;
    }

    // Execute build command if not phony
    if (!node->dep.is_phony && !node->dep.command.is_empty())
    {
        bld::log(bld::Log_type::INFO, "Building target: " + target);
        if (execute(node->dep.command) <= 0)
        {
            bld::log(bld::Log_type::ERROR, "Failed to build target: " + target);
            return false;
        }
    }
    else if (node->dep.is_phony)
        bld::log(bld::Log_type::INFO, "Phony target: " + target);
    else
        bld::log(bld::Log_type::WARNING, "No command for target: " + target);

    node->checked = true;
    return true;
}

bool bld::Dep_graph::detect_cycle(const std::string &target, std::unordered_set<std::string> &visited,
                                  std::unordered_set<std::string> &in_progress)
{
    if (in_progress.find(target) != in_progress.end())
        return true;  // Cycle detected

    if (visited.find(target) != visited.end())
        return false;  // Already processed

    auto it = nodes.find(target);
    if (it == nodes.end())
        return false;  // Target doesn't exist

    in_progress.insert(target);

    for (const auto &dep : it->second->dependencies)
        if (detect_cycle(dep, visited, in_progress))
            return true;

    in_progress.erase(target);
    visited.insert(target);
    return false;
}

bool bld::Dep_graph::build_parallel(const std::string &target, size_t thread_count)
{
    if (thread_count > std::thread::hardware_concurrency() - 1)
        thread_count = std::thread::hardware_concurrency() - 1;
    if (thread_count == 0)
        thread_count = 1;

    std::unordered_set<std::string> visited, in_progress;
    if (detect_cycle(target, visited, in_progress))
    {
        bld::log(bld::Log_type::ERROR, "Circular dependency detected for target: " + target);
        return false;
    }

    bld::log(bld::Log_type::INFO, "Building all targets in parallel using " + std::to_string(thread_count) + " threads");

    std::queue<std::string> ready_targets;
    if (!prepare_build_graph(target, ready_targets))
        return false;

    std::mutex queue_mutex, log_mutex;
    std::condition_variable cv;
    std::atomic<bool> build_failed{false};
    std::atomic<size_t> active_builds{0};
    std::atomic<size_t> completed_targets{0};
    const size_t total_targets = nodes.size();

    auto worker = [&]()
    {
        while (!build_failed)
        {
            std::string current_target;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (ready_targets.empty() && completed_targets < total_targets)
                {
                    cv.wait_for(lock, std::chrono::milliseconds(100),
                                [&]() { return !ready_targets.empty() || build_failed || completed_targets == total_targets; });
                }

                if (ready_targets.empty() || build_failed)
                    return;

                current_target = ready_targets.front();
                ready_targets.pop();
                nodes[current_target]->in_progress = true;
                active_builds++;
            }

            auto node = nodes[current_target].get();
            if (needs_rebuild(node))
            {
                if (!node->dep.is_phony && !node->dep.command.is_empty())
                {
                    {
                        std::lock_guard<std::mutex> log_lock(log_mutex);
                        bld::log(bld::Log_type::INFO, "Building target: " + current_target);
                    }

                    if (execute(node->dep.command) <= 0)
                    {
                        {
                            std::lock_guard<std::mutex> log_lock(log_mutex);
                            bld::log(bld::Log_type::ERROR, "Failed to build target: " + current_target);
                        }
                        build_failed = true;
                        cv.notify_all();
                        return;
                    }
                }
            }
            else
            {
                process_completed_target(current_target, ready_targets, queue_mutex, cv);
                completed_targets++;
                active_builds--;
                cv.notify_all();
            }
        }
    };

    std::vector<std::thread> workers;
    for (size_t i = 0; i < thread_count; ++i)
        workers.emplace_back(worker);

    for (auto &t : workers)
        if (t.joinable())
            t.join();

    return !build_failed;
}

bool bld::Dep_graph::prepare_build_graph(const std::string &target, std::queue<std::string> &ready_targets)
{
    auto it = nodes.find(target);
    if (it == nodes.end())
    {
        if (std::filesystem::exists(target))
        {
            if (checked_sources.find(target) == checked_sources.end())
            {
                bld::log(bld::Log_type::INFO, "Using existing source file: " + target);
                checked_sources.insert(target);
            }
            return true;
        }
        bld::log(bld::Log_type::ERROR, "Target not found: " + target);
        return false;
    }

    auto node = it->second.get();
    if (node->visited)
        return true;
    node->visited = true;

    // Process dependencies
    for (const auto &dep : node->dependencies)
    {
        if (!prepare_build_graph(dep, ready_targets))
            return false;

        // Only track node dependencies that actually need rebuilding
        if (nodes.find(dep) != nodes.end() && needs_rebuild(nodes[dep].get()))
            node->waiting_on.push_back(dep);
    }

    // Only add to ready queue if NEEDS rebuild and dependencies are met
    if (node->waiting_on.empty() && needs_rebuild(node))
        ready_targets.push(target);

    return true;
}

void bld::Dep_graph::process_completed_target(const std::string &target, std::queue<std::string> &ready_targets, std::mutex &queue_mutex,
                                              std::condition_variable &cv)
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    nodes[target]->checked = true;
    nodes[target]->in_progress = false;

    // Find all nodes that were waiting on this target
    for (const auto &node_pair : nodes)
    {
        auto &node = node_pair.second;
        if (!node->checked && !node->in_progress)
        {
            auto &waiting = node->waiting_on;
            waiting.erase(std::remove(waiting.begin(), waiting.end(), target), waiting.end());

            // If no more dependencies, add to ready queue
            if (waiting.empty())
                ready_targets.push(node_pair.first);
        }
    }
}

bool bld::Dep_graph::build_all_parallel(size_t thread_count)
{
    std::vector<std::string> root_targets;
    for (const auto &node : nodes)
    {
        bool is_dependency = false;
        for (const auto &other : nodes)
        {
            if (std::find(other.second->dep.dependencies.begin(), other.second->dep.dependencies.end(), node.first) !=
                other.second->dep.dependencies.end())
            {
                is_dependency = true;
                break;
            }
        }
        if (!is_dependency)
            root_targets.push_back(node.first);
    }

    // Create a master phony target that depends on all root targets
    add_phony("__master_target__", root_targets);
    bool result = build_parallel("__master_target__", thread_count);

    // Clean up the master target
    nodes.erase("__master_target__");
    return result;
}

#endif
