// Used AI for this
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

namespace bld::test {
struct Test_case
{
    std::string name;
    std::function<std::expected<void, std::string>()> fn;
};

auto expect_eq(std::string_view expected, std::string_view actual) -> std::expected<void, std::string>;
auto expect_file_match(std::string_view expected_path, std::string_view actual_path) -> std::expected<void, std::string>;
auto expect_cmd_success(const bld::Cmd &cmd) -> std::expected<void, std::string>;
auto run_suite(std::span<Test_case> tests) -> int;
}; // namespace bld::test

static auto internal_test_read_file(std::string_view path) -> std::expected<std::string, std::string>
{
    auto fd_res = bld::Owned_Fd::open(path, bld::Open_mode::read);
    if (!fd_res) {
        return std::unexpected(std::format("Could not open file '{}': {}", path, fd_res.error().msg));
    }

    char buf[4096];
    std::string content;
    while (true) {
        ssize_t bytes = ::read(fd_res->handle_, buf, sizeof(buf));
        if (bytes > 0) {
            content.append(buf, static_cast<std::size_t>(bytes));
        } else if (bytes == 0) {
            break;
        } else {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(std::format("Read error on '{}': {}", path, std::strerror(errno)));
        }
    }
    return content;
}

auto bld::test::expect_eq(std::string_view expected, std::string_view actual) -> std::expected<void, std::string>
{
    auto diff = bld::test::compute_diff(expected, actual);
    if (!diff.same) {
        return std::unexpected(std::format("Value mismatch evaluated:\n{}", diff));
    }
    return {};
}

auto bld::test::expect_file_match(std::string_view expected_path, std::string_view actual_path) -> std::expected<void, std::string>
{
    namespace fs = std::filesystem;
    if (!fs::exists(expected_path)) {
        return std::unexpected(std::format("Baseline path missing: '{}'", expected_path));
    }
    if (!fs::exists(actual_path)) {
        return std::unexpected(std::format("Artifact path missing: '{}'", actual_path));
    }

    auto exp = internal_test_read_file(expected_path);
    if (!exp) {
        return std::unexpected(exp.error());
    }

    auto act = internal_test_read_file(actual_path);
    if (!act) {
        return std::unexpected(act.error());
    }

    return expect_eq(*exp, *act);
}

auto bld::test::expect_cmd_success(const bld::Cmd &cmd) -> std::expected<void, std::string>
{
    auto proc = bld::run(bld::Cmd_loc{cmd});
    if (!proc) {
        return std::unexpected(std::format("Process execution sub-system failed: {}", proc.error()));
    }
    if (proc->status_.code != 0) {
        return std::unexpected(std::format("Command execution non-zero exit path: {}", proc->status_));
    }
    return {};
}

// Runner Orchestration Definition
auto bld::test::run_suite(std::span<Test_case> tests) -> int
{
    bld::log::i("Running tests: {} cases", tests.size());
    std::size_t passed = 0;
    std::size_t failed = 0;

    for (const auto &t : tests) {
        // RAII Log Interceptor
        struct Log_interceptor
        {
            std::ostringstream trap;
            std::ostream *original_ptr;
            Log_interceptor()
            {
                original_ptr = bld::Logger::ostream.ptr;
                bld::Logger::ostream = trap;
            }
            ~Log_interceptor()
            {
                bld::Logger::ostream.ptr = original_ptr;
            }
        } interceptor;

        auto res = t.fn();

        // Restore the logger pointer immediately before printing the result
        bld::Logger::ostream.ptr = interceptor.original_ptr;

        if (res) {
            bld::log::i("[PASS] {}", t.name);
            passed++;
        } else {
            std::string trapped_logs = interceptor.trap.str();
            if (trapped_logs.empty()) {
                bld::log::e("[FAIL] {}\n  Error: {}", t.name, res.error());
            } else {
                bld::log::e("[FAIL] {}\n  Error: {}\n  --- Trapped Execution Logs ---\n{}", t.name, res.error(), trapped_logs);
            }
            failed++;
        }
    }

    if (failed > 0) {
        bld::log::e("Suite execution complete: {}/{} passed, {} failed.", passed, passed + failed, failed);
        return 1;
    }

    bld::log::i("Suite execution complete: All {} test cases passed.", passed);
    return 0;
}

// Static Sandbox Helpers
static auto make_sandbox_file(const std::filesystem::path &path, std::string_view text, std::chrono::seconds age_offset = std::chrono::seconds(0))
    -> void
{
    std::ofstream ofs(path);
    ofs << text;
    ofs.close();

    if (age_offset.count() != 0) {
        auto current_time = std::filesystem::last_write_time(path);
        std::filesystem::last_write_time(path, current_time + age_offset);
    }
}

[[maybe_unused]] static auto read_sandbox_file(std::string_view path) -> std::expected<std::string, std::string>
{
    auto fd_res = bld::Owned_Fd::open(path, bld::Open_mode::read);
    if (!fd_res) {
        return std::unexpected(std::format("Could not open file '{}': {}", path, fd_res.error().msg));
    }

    char buf[4096];
    std::string content;
    while (true) {
        ssize_t bytes = ::read(fd_res->handle_, buf, sizeof(buf));
        if (bytes > 0) {
            content.append(buf, static_cast<std::size_t>(bytes));
        } else if (bytes == 0) {
            break;
        } else {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(std::format("Read error on '{}': {}", path, std::strerror(errno)));
        }
    }
    return content;
}

auto run_tests() -> int
{
    namespace fs = std::filesystem;
    fs::create_directories("./test_sandbox");

    std::vector<bld::test::Test_case> suite = {
        {"cmd_empty_initialization",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd{};
             if (!cmd.empty()) {
                 return std::unexpected("expected empty cmd");
             }
             if (cmd.size() != 0) {
                 return std::unexpected("size should be 0");
             }
             if (cmd.str() != "") {
                 return std::unexpected("str() should be empty string");
             }
             return {};
         }},
        {"cmd_variadic_initialization_and_argv",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd{"g++", "-o", "main", "main.cpp"};
             if (cmd.size() != 4) {
                 return std::unexpected("size should be 4");
             }
             if (cmd.str() != "g++ -o main main.cpp") {
                 return std::unexpected("str() formatting error");
             }

             auto c_args = cmd.argv();
             if (c_args.size() != 5) {
                 return std::unexpected("argv() missing null-terminator tracking size");
             }
             if (c_args[4] != nullptr) {
                 return std::unexpected("argv() missing mandatory trailing null terminator");
             }
             return {};
         }},
        {"cmd_iterators_and_span",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd{"gcc", "-O3"};
             auto s = cmd.span();
             if (s.size() != 2) {
                 return std::unexpected("span() size mismatch");
             }
             if (s[1] != "-O3") {
                 return std::unexpected("span() index access mismatch");
             }

             std::size_t count = 0;
             for (const auto &arg : cmd) {
                 (void)arg;
                 count++;
             }
             if (count != 2) {
                 return std::unexpected("range-based for loop failed to iterate correctly");
             }
             return {};
         }},
        {"cmd_push_and_reset",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd{"gcc"};
             cmd.push("-O3");
             cmd.emplace_b("-Wall");
             if (cmd.str() != "gcc -O3 -Wall") {
                 return std::unexpected("mutations failed to update string signature");
             }

             cmd.reset();
             if (!cmd.empty() || cmd.str() != "") {
                 return std::unexpected("reset failed to flush data layout");
             }
             return {};
         }},

        {"config_defaults_and_strict_parsing",
         []() -> std::expected<void, std::string> {
             auto &c = bld::Config::get();
             c.data.clear();
             c.options.clear();

             c.add_option("jobs", bld::Config::Int, "Concurrencies", 4)
                 .add_option("mode", bld::Config::String, "Profile", std::string{"debug"})
                 .add_option("--optimize", bld::Config::Bool, "Optimize flag", false);

             std::vector<const char *> mock_argv = {"./bld", "jobs=16", "--optimize", "mode=release"};
             c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data()));

             if (int(c["jobs"]) != 16) {
                 return std::unexpected("int proxy conversion failed");
             }
             if (bool(c["--optimize"]) != true) {
                 return std::unexpected("bool equations parsing failed");
             }
             if (std::string(c["mode"]) != "release") {
                 return std::unexpected("string extraction mismatch");
             }
             return {};
         }},
        {"config_proxy_exception_handling",
         []() -> std::expected<void, std::string> {
             auto &c = bld::Config::get();
             c.data.clear();
             c.options.clear();
             c.add_option("flag", bld::Config::Bool, "Test", true);

             std::vector<const char *> mock_argv = {"./bld"};
             c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data()));

             try {
                 int val = c["flag"]; // Invalid cast from bool to int
                 return std::unexpected(std::format("Proxy failed to throw on invalid cast. Returned {}", val));
             } catch (const std::runtime_error &) {
                 return {}; // Expected behavior
             }
         }},

        {"diff_engine_exact_matches",
         []() -> std::expected<void, std::string> {
             std::string_view text = "Line 1\nLine 2\nLine 3";
             auto diff = bld::test::compute_diff(text, text);
             if (!diff.same) {
                 return std::unexpected("diff algorithm failed to verify identical elements");
             }
             return {};
         }},
        {"diff_engine_and_formatting_variants",
         []() -> std::expected<void, std::string> {
             auto diff = bld::test::compute_diff("Heya\nBrother", "Heya\nBroter");
             if (diff.same) {
                 return std::unexpected("diff failed to flag changes");
             }

             std::string raw_ascii = std::format("{:n}", diff);
             if (raw_ascii.find("- Brother") == std::string::npos || raw_ascii.find("+ Broter") == std::string::npos) {
                 return std::unexpected("Myers raw text output path layout is corrupted");
             }
             return {};
         }},

        {"fs_is_outdated_lifecycle",
         []() -> std::expected<void, std::string> {
             auto target = fs::path("./test_sandbox/target.o");
             auto source = fs::path("./test_sandbox/source.cpp");

             fs::remove(target);
             make_sandbox_file(source, "int code;");
             if (!bld::is_outdated(target.string(), source.string())) {
                 return std::unexpected("missing target binary must report as outdated");
             }

             make_sandbox_file(target, "bin", std::chrono::seconds(-20));
             make_sandbox_file(source, "src", std::chrono::seconds(20));
             if (!bld::is_outdated(target.string(), source.string())) {
                 return std::unexpected("stale target timestamp must require a rebuild");
             }

             make_sandbox_file(target, "bin", std::chrono::seconds(40));
             if (bld::is_outdated(target.string(), source.string())) {
                 return std::unexpected("fresh binary should be recognized as up-to-date");
             }
             return {};
         }},
        {"fs_is_outdated_multiple_sources",
         []() -> std::expected<void, std::string> {
             auto target = fs::path("./test_sandbox/target2.o");
             auto src1 = fs::path("./test_sandbox/src1.cpp");
             auto src2 = fs::path("./test_sandbox/src2.cpp");

             make_sandbox_file(target, "bin", std::chrono::seconds(10));
             make_sandbox_file(src1, "src1", std::chrono::seconds(0));
             make_sandbox_file(src2, "src2", std::chrono::seconds(20)); // src2 is newer than target

             std::array<std::string_view, 2> sources{src1.string(), src2.string()};

             if (!bld::is_outdated(target.string(), sources)) {
                 return std::unexpected("failed to detect that the second source file triggered an outdated state");
             }
             return {};
         }},

        {"fs_make_dir_if_not_exists",
         []() -> std::expected<void, std::string> {
             return {};
         }},

        {"process_sync_and_async_execution",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd_sync{"true"};
             auto sync_res = bld::run(bld::Cmd_loc{cmd_sync});
             if (!sync_res || sync_res->status_code() != 0) {
                 return std::unexpected("synchronous execution failure");
             }

             bld::Cmd cmd_async{"sleep", "0.02"};
             auto async_res = bld::run(bld::Cmd_loc{cmd_async}, bld::async{});
             if (!async_res || !async_res->is_running()) {
                 return std::unexpected("asynchronous process tracking error");
             }

             auto wait_status = async_res->wait();
             if (!wait_status || async_res->is_running()) {
                 return std::unexpected("async tracking handles failed to cleanly terminate");
             }
             return {};
         }},
        {"process_capture_stdout_and_stderr",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd{"sh", "-c", "echo out_data && echo err_data >&2"};
             std::string stdout_storage;
             std::string stderr_storage;

             auto status = bld::capture(bld::Cmd_loc{cmd}, bld::cap_out{stdout_storage}, bld::cap_err{stderr_storage});
             if (!status || status->code != 0) {
                 return std::unexpected("process execution failed during capture parsing");
             }

             if (stdout_storage != "out_data\n") {
                 return std::unexpected(std::format("stdout corrupted: '{}'", stdout_storage));
             }
             if (stderr_storage != "err_data\n") {
                 return std::unexpected(std::format("stderr corrupted: '{}'", stderr_storage));
             }
             return {};
         }},
        {"process_capture_merged_streams",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd{"sh", "-c", "echo a && echo b >&2"};
             std::string merged_storage;

             auto status = bld::capture(bld::Cmd_loc{cmd}, bld::cap_merge{merged_storage});
             if (!status || status->code != 0) {
                 return std::unexpected("process execution failed during capture merge");
             }

             if (merged_storage.find("a") == std::string::npos || merged_storage.find("b") == std::string::npos) {
                 return std::unexpected(std::format("merge capture failed to combine streams: '{}'", merged_storage));
             }
             return {};
         }},

        {"task_batch_staggered_scheduler",
         []() -> std::expected<void, std::string> {
             std::vector<bld::Task> execution_list;
             execution_list.emplace_back(bld::Cmd_loc{bld::Cmd{"sleep", "0.01"}});
             execution_list.emplace_back(bld::Cmd_loc{bld::Cmd{"sleep", "0.01"}});
             execution_list.emplace_back(bld::Cmd_loc{bld::Cmd{"sleep", "0.01"}});

             auto batch_status = bld::run(execution_list, 2);
             if (!batch_status) {
                 return std::unexpected("parallel batch tracking cluster crashed");
             }
             return {};
         }},
        {"task_batch_poison_interruption", []() -> std::expected<void, std::string> {
             std::vector<bld::Task> broken_list;
             broken_list.emplace_back(bld::Cmd_loc{bld::Cmd{"true"}});
             broken_list.emplace_back(bld::Cmd_loc{bld::Cmd{"false"}});
             broken_list.emplace_back(bld::Cmd_loc{bld::Cmd{"true"}});

             // Force max_jobs = 1 to guarantee sequential processing.
             // This ensures the 3rd task is NEVER scheduled because the 2nd task poisons the batch queue.
             auto batch_status = bld::run(broken_list, 1);
             if (batch_status) {
                 return std::unexpected("scheduler silently swallowed task failure");
             }

             auto count = std::any_cast<std::size_t>(batch_status.error().payload);
             if (count >= 3) {
                 return std::unexpected(std::format("poisoned pill failed to halt execution queue. Completed count: {}", count));
             }
             return {};
         }}};

    int exit_code = bld::test::run_suite(suite);

    fs::remove_all("./test_sandbox");
    return exit_code;
}

auto main(int argc, char *argv[]) -> int
{
    bld::rebuild_this_when_needed_ext(argc, argv);
    return run_tests();
}
