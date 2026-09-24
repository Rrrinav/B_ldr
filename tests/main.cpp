// Used AI for this
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#define B_LDR_IMPLEMENTATION
#include "../b_ldr.hpp"

// Modifier-category classification guards: these drive the guided static_asserts
// in run/capture/wait_all/Task/run_new. If a modifier ever gains/loses an
// overload, these fail at compile time next to the mistake.
static_assert(bld::Config_modifier_c<bld::io_in>);
static_assert(bld::Config_modifier_c<bld::label>);
static_assert(bld::Config_modifier_c<bld::io_out>);
static_assert(bld::Config_modifier_c<bld::io_err>);
static_assert(bld::Config_modifier_c<bld::io_out_err>);
static_assert(!bld::Config_modifier_c<bld::jobs>);
static_assert(bld::Config_modifier_c<bld::dry_run>);
static_assert(bld::Config_modifier_c<bld::in_str>);
static_assert(bld::Run_modifier_c<bld::jobs>);
static_assert(bld::Run_modifier_c<bld::keep_going>);
static_assert(!bld::Run_modifier_c<bld::io_out>);
static_assert(!bld::Run_modifier_c<bld::io_in>);
static_assert(!bld::Run_modifier_c<bld::io_out_err>);
static_assert(bld::Capture_modifier_c<bld::in_str>);
static_assert(bld::Capture_modifier_c<bld::label>);
static_assert(bld::Capture_modifier_c<bld::io_in>);
static_assert(!bld::Capture_modifier_c<bld::jobs>);
static_assert(bld::Capture_modifier_c<bld::dry_run>);
static_assert(!bld::Capture_modifier_c<bld::io_out>);
static_assert(!bld::Capture_modifier_c<bld::io_err>);
static_assert(!bld::Capture_modifier_c<bld::io_out_err>);

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

struct Test_file_res
{
    std::string function;
    int total{};
    int failed{};

    std::vector<int> failed_indices;
    std::vector<std::string> failed_messages;
};

auto parse_results(std::istream &in) -> std::expected<std::vector<Test_file_res>, std::string>
{
    std::vector<Test_file_res> results;
    std::string line;

    auto expect_line = [&](std::string_view expected) -> std::expected<void, std::string> {
        if (!std::getline(in, line)) {
            return std::unexpected(std::format("Expected '{}', reached EOF.", expected));
        }

        if (line != expected) {
            return std::unexpected(std::format("Expected '{}', got '{}'.", expected, line));
        }

        return {};
    };

    while (true) {
        if (!std::getline(in, line)) {
            break;
        }

        if (line.empty()) {
            continue;
        }

        if (line != "FUNCTION") {
            return std::unexpected(std::format("Expected 'FUNCTION', got '{}'.", line));
        }

        Test_file_res r{};

        if (!std::getline(in, r.function)) {
            return std::unexpected("Missing function name.");
        }

        if (auto e = expect_line("TOTAL"); !e) {
            return std::unexpected(e.error());
        }

        if (!std::getline(in, line)) {
            return std::unexpected("Missing total.");
        }
        r.total = std::stoi(line);

        if (auto e = expect_line("FAILED"); !e) {
            return std::unexpected(e.error());
        }

        if (!std::getline(in, line)) {
            return std::unexpected("Missing failed count.");
        }
        r.failed = std::stoi(line);

        if (auto e = expect_line("FAILED_INDICES"); !e) {
            return std::unexpected(e.error());
        }

        if (!std::getline(in, line)) {
            return std::unexpected("Missing failed indices.");
        }

        if (!line.empty()) {
            std::stringstream ss(line);
            std::string tok;

            while (std::getline(ss, tok, ',')) {
                r.failed_indices.push_back(std::stoi(tok));
            }
        }

        if (auto e = expect_line("FAILED_MESSAGES"); !e) {
            return std::unexpected(e.error());
        }

        while (true) {
            if (!std::getline(in, line)) {
                return std::unexpected("Unexpected EOF while reading failure messages.");
            }

            if (line == "END") {
                break;
            }

            r.failed_messages.push_back(line);
        }

        if (r.failed != static_cast<int>(r.failed_indices.size())) {
            return std::unexpected(std::format("{}: failed count ({}) != indices ({})", r.function, r.failed, r.failed_indices.size()));
        }

        if (r.failed != static_cast<int>(r.failed_messages.size())) {
            return std::unexpected(std::format("{}: failed count ({}) != messages ({})", r.function, r.failed, r.failed_messages.size()));
        }

        results.push_back(std::move(r));
    }

    return results;
}
}; // namespace bld::test

static auto internal_test_read_file(std::string_view path) -> std::expected<std::string, std::string>
{
    auto res = bld::fs::read_file(path);
    if (!res) {
        return std::unexpected(std::format("Could not read file '{}': {}", path, res.error().msg));
    }
    return *res;
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
    return internal_test_read_file(path);
}

namespace {
#ifdef _WIN32
inline bld::Cmd true_cmd()  { return bld::Cmd{"cmd", "/c", "exit", "0"}; }
inline bld::Cmd false_cmd() { return bld::Cmd{"cmd", "/c", "exit", "1"}; }
inline bld::Cmd sleep_cmd() { return bld::Cmd{"powershell", "-NoProfile", "-Command", "Start-Sleep -Milliseconds 300"}; }
inline bld::Cmd echo_cmd(std::string_view out_data, std::string_view err_data)
{
    return bld::Cmd{"powershell", "-NoProfile", "-Command",
                    std::format("[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;Write-Output {};[Console]::Error.WriteLine('{}')", out_data, err_data)};
}
#else
inline bld::Cmd true_cmd()  { return bld::Cmd{"true"}; }
inline bld::Cmd false_cmd() { return bld::Cmd{"false"}; }
inline bld::Cmd sleep_cmd() { return bld::Cmd{"sleep", "0.02"}; }
inline bld::Cmd echo_cmd(std::string_view out_data, std::string_view err_data)
{
    return bld::Cmd{"sh", "-c", std::format("echo {} && echo {} >&2", out_data, err_data)};
}
#endif
} // namespace

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
             std::ignore = c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data()));

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
              std::ignore = c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data()));

              try {
                  int val = c["flag"]; // Invalid cast from bool to int
                  return std::unexpected(std::format("Proxy failed to throw on invalid cast. Returned {}", val));
              } catch (const std::runtime_error &) {
                  return {}; // Expected behavior
              }
         }},
        {"config_dash_normalization",
         []() -> std::expected<void, std::string> {
              auto &c = bld::Config::get();
              c.data.clear();
              c.options.clear();
              c.add_option("jobs", bld::Config::Int, "Concurrencies", 4)
                  .add_option("verbose", bld::Config::Bool, "Verbose", false);

              std::vector<const char *> mock_argv = {"./bld", "--jobs=8", "--verbose"};
              if (auto r = c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data())); !r) {
                  return std::unexpected(std::format("parse failed: {}", r.error()));
              }
              if (int(c["jobs"]) != 8) {
                  return std::unexpected("--jobs=8 did not normalize to jobs");
              }
              if (int(c["--jobs"]) != 8) {
                  return std::unexpected("lookup with dashes should hit the same key");
              }
              if (bool(c["verbose"]) != true) {
                  return std::unexpected("--verbose did not set verbose");
              }
              return {};
         }},
        {"config_space_separated_values",
         []() -> std::expected<void, std::string> {
              auto &c = bld::Config::get();
              c.data.clear();
              c.options.clear();
              c.add_option("jobs", bld::Config::Int, "Concurrencies", 4)
                  .add_option("mode", bld::Config::String, "Profile", std::string{"debug"}, {"debug", "release"})
                  .add_option("ratio", bld::Config::Double, "Ratio", 1.0);

              std::vector<const char *> mock_argv = {"./bld", "jobs", "7", "--mode", "release", "ratio", "-2.5"};
              if (auto r = c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data())); !r) {
                  return std::unexpected(std::format("parse failed: {}", r.error()));
              }
              if (int(c["jobs"]) != 7 || std::string(c["mode"]) != "release" || double(c["ratio"]) != -2.5) {
                  return std::unexpected("space-separated values misparsed");
              }
              return {};
         }},
        {"config_strict_unknown_rejected",
         []() -> std::expected<void, std::string> {
              auto &c = bld::Config::get();
              c.data.clear();
              c.options.clear();
              c.add_option("jobs", bld::Config::Int, "Concurrencies", 4);

              for (auto args : {std::vector<const char *>{"./bld", "job=8"},
                                std::vector<const char *>{"./bld", "--nope"},
                                std::vector<const char *>{"./bld", "jobs"},
                                std::vector<const char *>{"./bld", "jobs=abc"}}) {
                  c.data.clear();
                  c.options.clear();
                  c.add_option("jobs", bld::Config::Int, "Concurrencies", 4);
                  if (c.parse(args.size(), const_cast<char **>(args.data()))) {
                      return std::unexpected(std::format("parse unexpectedly accepted '{}'", args[1]));
                  }
              }
              return {};
         }},
        {"config_types_choices_and_help",
         []() -> std::expected<void, std::string> {
              auto &c = bld::Config::get();
              c.data.clear();
              c.options.clear();
              c.add_option("jobs", bld::Config::Int, "Concurrencies", 4)
                  .add_option("verbose", bld::Config::Bool, "Verbose", false)
                  .add_option("mode", bld::Config::String, "Profile", std::string{"debug"}, {"debug", "release"})
                  .add_option("src", bld::Config::String_arr, "Sources", std::vector<std::string>{});

              std::vector<const char *> mock_argv = {"./bld", "src=a.cpp", "--src", "b.cpp", "verbose=YES", "mode=release"};
              if (auto r = c.parse(mock_argv.size(), const_cast<char **>(mock_argv.data())); !r) {
                  return std::unexpected(std::format("parse failed: {}", r.error()));
              }
              auto srcs = std::vector<std::string>(c["src"]);
              if (srcs.size() != 2 || srcs[0] != "a.cpp" || srcs[1] != "b.cpp") {
                  return std::unexpected("String_arr did not accumulate both forms");
              }
              if (bool(c["verbose"]) != true) {
                  return std::unexpected("case-insensitive bool failed");
              }
              std::vector<const char *> bad = {"./bld", "mode=fast"};
              c.data.clear();
              c.options.clear();
              c.add_option("mode", bld::Config::String, "Profile", std::string{"debug"}, {"debug", "release"});
              if (c.parse(bad.size(), const_cast<char **>(bad.data()))) {
                  return std::unexpected("invalid choice unexpectedly accepted");
              }
              std::vector<const char *> help = {"./bld", "--help"};
              if (auto r = c.parse(help.size(), const_cast<char **>(help.data())); !r || !r->help_requested) {
                  return std::unexpected("--help did not request help");
              }
              if (auto r = c.parse(0, nullptr); !r) {
                  return std::unexpected("argc==0 should parse defaults");
              }
              if (c.parse(1, nullptr)) {
                  return std::unexpected("null argv with argc>0 should fail");
              }
              return {};
         }},
        {"env_roundtrip_and_validation",
         []() -> std::expected<void, std::string> {
              const std::string key = "BLD_TEST_ROUNDTRIP_XYZ";
              std::ignore = bld::env::unset(key);
              if (auto s = bld::env::set(key, "1"); !s) {
                  return std::unexpected(std::format("set failed: {}", s.error()));
              }
              if (!bld::env::has(key) || bld::env::get(key) != "1" || bld::env::get_or(key, "d") != "1") {
                  return std::unexpected("has/get/get_or mismatch after set");
              }
              if (auto s = bld::env::set(key, "2", false); !s) {
                  return std::unexpected("no-overwrite set failed");
              }
              if (bld::env::get(key) != "1") {
                  return std::unexpected("overwrite=false clobbered the value");
              }
              if (auto u = bld::env::unset(key); !u) {
                  return std::unexpected(std::format("unset failed: {}", u.error()));
              }
              if (bld::env::has(key) || bld::env::get(key).has_value() || bld::env::get_or(key, "d") != "d") {
                  return std::unexpected("value survived unset");
              }
              if (bld::env::set("", "x") || bld::env::set("a=b", "x") || bld::env::unset("")) {
                  return std::unexpected("invalid keys unexpectedly accepted");
              }
              auto all = bld::env::get_all();
              if (all.find("PATH") == all.end() && all.find("Path") == all.end()) {
                  return std::unexpected("get_all missed PATH");
              }
              return {};
         }},
        {"time_format_and_stamp",
         []() -> std::expected<void, std::string> {
              using namespace std::chrono_literals;
              if (bld::time::format(0ns) != "0ns") {
                  return std::unexpected("format(0ns) wrong");
              }
              if (bld::time::format(1500ns) != "1.5us") {
                  return std::unexpected(std::format("format(1500ns) wrong: '{}'", bld::time::format(1500ns)));
              }
              if (bld::time::format(2500000ns) != "2.50ms") {
                  return std::unexpected(std::format("format(2.5ms) wrong: '{}'", bld::time::format(2500000ns)));
              }
              if (bld::time::format(3000000000ns) != "3.000s") {
                  return std::unexpected(std::format("format(3s) wrong: '{}'", bld::time::format(3000000000ns)));
              }
              bld::time::stamp s;
              if (s.elapsed().count() < 0 || bld::time::since(s).count() < 0) {
                  return std::unexpected("negative elapsed");
              }
              if (s.reset().count() < 0 || s.elapsed().count() < 0) {
                  return std::unexpected("reset misbehaved");
              }
              return {};
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

             std::array<std::string, 2> sources{src1.string(), src2.string()};

             if (!bld::is_outdated(target.string(), sources)) {
                 return std::unexpected("failed to detect that the second source file triggered an outdated state");
             }
             return {};
         }},

         {"test_fs_functions",
         []() -> std::expected<void, std::string> {
              bld::Cmd cmd{"g++", "-o", "./test_fs", "tests/fs/main.cpp", "-std=c++23", "-O3", "-Wall", "-Wextra", "-I."};
#ifdef _WIN32
#ifdef __GNUC__
              cmd.push("-lstdc++exp");
#endif
#endif
              if (auto built = bld::run(cmd); !built) {
                  return std::unexpected(std::format("fs test build failed: {}", built.error()));
              }
              // Just becuase I wanted to supress the output
#ifdef _WIN32
              auto cap = bld::capture(bld::Cmd{"./test_fs.exe"});
#else
              auto cap = bld::capture(bld::Cmd{"./test_fs"});
#endif
              if (!cap) {
                  return std::unexpected(std::format("fs test run failed: {}", cap.error()));
              }
              std::expected<std::vector<bld::test::Test_file_res>, std::string> parsed;
              {
                  std::ifstream f("./tests/fs/out");
                  parsed = bld::test::parse_results(f);
              }
              std::filesystem::remove_all("./tests/fs/out");
              std::filesystem::remove_all("./test_fs");
              if (!parsed) {
                  return std::unexpected(parsed.error());
              }

              std::string err{};
              bool failed{false};

              for (const auto &suite : *parsed) {
                  if (suite.failed != 0) {
                      failed = true;
                      err += std::format("{} failed ({}/{})\n", suite.function, suite.failed, suite.total);

                      for (std::size_t i = 0; i < suite.failed_messages.size(); ++i) {
                          err += std::format("  [{}] {}\n", suite.failed_indices[i], suite.failed_messages[i]);
                      }
                  }
              }

              if (failed) {
                  return std::unexpected(err);
              } else {
                  return {};
              }
          }},
        {"test_str_functions",
         []() -> std::expected<void, std::string> {
              bld::Cmd cmd{"g++", "-o", "./test_str", "tests/str/main.cpp", "-std=c++23", "-O3", "-Wall", "-Wextra", "-I."};

#ifdef _WIN32
#ifdef __GNUC__
              cmd.push("-lstdc++exp");
#endif
#endif
              if (auto built = bld::run(cmd); !built)
              {
                  return std::unexpected(std::format("str test build failed: {}", built.error()));
              }
#ifdef _WIN32
              auto cap = bld::capture(bld::Cmd{"./test_str.exe"});
#else
              auto cap = bld::capture(bld::Cmd{"./test_str"});
#endif
              if (!cap) {
                  return std::unexpected(std::format("str test run failed: {}", cap.error()));
              }
              std::expected<std::vector<bld::test::Test_file_res>, std::string> parsed;
              {
                  std::ifstream f("./tests/str/out");
                  parsed = bld::test::parse_results(f);
              }
              std::filesystem::remove_all("./tests/str/out");
              std::filesystem::remove_all("./test_str");
              if (!parsed) {
                  return std::unexpected(parsed.error());
              }

              std::string err{};
              bool failed{false};

              for (const auto &suite : *parsed) {
                  if (suite.failed != 0) {
                      failed = true;
                      err += std::format("{} failed ({}/{})\n", suite.function, suite.failed, suite.total);

                      for (std::size_t i = 0; i < suite.failed_messages.size(); ++i) {
                          err += std::format("  [{}] {}\n", suite.failed_indices[i], suite.failed_messages[i]);
                      }
                  }
              }

              if (failed) {
                  return std::unexpected(err);
              } else {
                  return {};
              }
          }},

        {"process_sync_and_async_execution",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd_sync = true_cmd();
             auto sync_res = bld::run(bld::Cmd_loc{cmd_sync});
             if (!sync_res || sync_res->status_code() != 0) {
                 return std::unexpected("synchronous execution failure");
             }

             bld::Cmd cmd_async = sleep_cmd();
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
             bld::Cmd cmd = echo_cmd("out_data", "err_data");

             auto merged = bld::capture(bld::Cmd_loc{cmd});
             if (!merged) {
                 return std::unexpected(std::format("process execution failed during capture parsing: {}", merged.error()));
             }

             if (merged->find("out_data") == std::string::npos) {
                 return std::unexpected(std::format("stdout missing from merged capture: '{}'", *merged));
             }
             if (merged->find("err_data") == std::string::npos) {
                 return std::unexpected(std::format("stderr missing from merged capture: '{}'", *merged));
             }
             return {};
         }},
        {"process_capture_merged_streams",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd = echo_cmd("a", "b");

             auto merged = bld::capture(bld::Cmd_loc{cmd});
             if (!merged) {
                 return std::unexpected(std::format("process execution failed during capture merge: {}", merged.error()));
             }

             if (merged->find("a") == std::string::npos || merged->find("b") == std::string::npos) {
                 return std::unexpected(std::format("merge capture failed to combine streams: '{}'", *merged));
             }
             return {};
         }},
        {"run_io_out_captures_stdout_only",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd = echo_cmd("out_data", "err_data");
             std::string out;

             auto proc = bld::run(bld::Cmd_loc{cmd}, bld::io_out{&out});
             if (!proc) {
                 return std::unexpected(std::format("run with io_out failed: {}", proc.error()));
             }
             // Separate by default: stdout captured, stderr untouched by the string.
             if (out != "out_data\n") {
                 return std::unexpected(std::format("stdout capture wrong: '{}'", out));
             }
             return {};
         }},
        {"run_io_err_captures_stderr_only",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd = echo_cmd("out_data", "err_data");
             std::string err;

             auto proc = bld::run(bld::Cmd_loc{cmd}, bld::io_err{&err});
             if (!proc) {
                 return std::unexpected(std::format("run with io_err failed: {}", proc.error()));
             }
             if (err != "err_data\n") {
                 return std::unexpected(std::format("stderr capture wrong: '{}'", err));
             }
             return {};
         }},
        {"run_io_out_err_merges",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd = echo_cmd("out_data", "err_data");
             std::string merged;

             auto proc = bld::run(bld::Cmd_loc{cmd}, bld::io_out_err{&merged});
             if (!proc) {
                 return std::unexpected(std::format("run with io_out_err failed: {}", proc.error()));
             }
             if (merged.find("out_data") == std::string::npos || merged.find("err_data") == std::string::npos) {
                 return std::unexpected(std::format("merged string capture wrong: '{}'", merged));
             }
             return {};
         }},
        {"run_split_io_out_and_io_err",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd = echo_cmd("out_data", "err_data");
             std::string out, err;

             auto proc = bld::run(bld::Cmd_loc{cmd}, bld::io_out{&out}, bld::io_err{&err});
             if (!proc) {
                 return std::unexpected(std::format("run with split strings failed: {}", proc.error()));
             }
             if (out != "out_data\n") {
                 return std::unexpected(std::format("split stdout wrong: '{}'", out));
             }
             if (err != "err_data\n") {
                 return std::unexpected(std::format("split stderr wrong: '{}'", err));
             }
             return {};
         }},
        {"run_io_out_err_fd_merges",
         []() -> std::expected<void, std::string> {
             bld::Cmd cmd = echo_cmd("out_data", "err_data");
             auto fd = bld::Owned_Fd::open("test_sandbox/oefd.txt", bld::Open_mode::write);
             if (!fd) {
                 return std::unexpected(std::format("could not open sandbox file: {}", fd.error()));
             }

             auto proc = bld::run(bld::Cmd_loc{cmd}, bld::io_out_err{*fd});
             if (!proc) {
                 return std::unexpected(std::format("run with io_out_err failed: {}", proc.error()));
             }
             auto txt = bld::fs::read_file("test_sandbox/oefd.txt");
             if (!txt) {
                 return std::unexpected("could not read back merged fd output");
             }
             if (txt->find("out_data") == std::string::npos || txt->find("err_data") == std::string::npos) {
                 return std::unexpected(std::format("merged fd output wrong: '{}'", *txt));
             }
             return {};
         }},
        {"run_str_capture_async_detached",
         []() -> std::expected<void, std::string> {
             // Capture pipes live in the Proc: strings are complete after wait()
             // even though the run returned while the child was starting.
             std::string out;
             auto proc = bld::run(bld::Cmd_loc{echo_cmd("late_data", "x")}, bld::async{}, bld::io_out{&out});
             if (!proc) {
                 return std::unexpected(std::format("async spawn with io_out failed: {}", proc.error()));
             }
             auto status = proc->wait();
             if (!status || status->code != 0) {
                 return std::unexpected("async captured proc did not exit cleanly");
             }
             if (out != "late_data\n") {
                 return std::unexpected(std::format("async string capture wrong: '{}'", out));
             }
             return {};
         }},
        {"unified_run_respects_graph_dependencies",
         []() -> std::expected<void, std::string> {
              bld::Plan plan;
              plan.add("prepare", sleep_cmd());
              plan.add("consume", true_cmd());
              plan.after("consume", "prepare");
              auto report = bld::run(plan, bld::jobs{2});
              if (!report || report->ran != 2 || report->failed != 0) {
                  return std::unexpected("dependency plan did not complete through the unified scheduler");
              }
              return {};
         }},
        {"run_file_routing_to_paths",
         []() -> std::expected<void, std::string> {
              bld::Cmd cmd = echo_cmd("out_data", "err_data");
              auto proc = bld::run(
                  bld::Cmd_loc{cmd}, bld::io_out{"test_sandbox/route_out.txt"}, bld::io_err{"test_sandbox/route_err.txt"});
              if (!proc) {
                  return std::unexpected(std::format("run with file routing failed: {}", proc.error()));
              }
              auto out = bld::fs::read_file("test_sandbox/route_out.txt");
              auto err = bld::fs::read_file("test_sandbox/route_err.txt");
              if (!out || out->find("out_data") == std::string::npos) {
                  return std::unexpected("stdout file routing wrong");
              }
              if (!err || err->find("err_data") == std::string::npos) {
                  return std::unexpected("stderr file routing wrong");
              }
              return {};
         }},
        {"run_stdin_content",
         []() -> std::expected<void, std::string> {
              // Plumbing everywhere: a child that ignores stdin must still
              // succeed (and an early exit must not kill us via SIGPIPE).
              if (auto proc = bld::run(bld::Cmd_loc{true_cmd()}, bld::in_str{"hello"}); !proc) {
                  return std::unexpected(std::format("run with in_str failed: {}", proc.error()));
              }
              // Empty content behaves like unset (inherit), mirroring capture().
              if (auto proc = bld::run(bld::Cmd_loc{true_cmd()}, bld::in_str{""}); !proc) {
                  return std::unexpected(std::format("run with empty in_str failed: {}", proc.error()));
              }
#ifndef _WIN32
              // Content roundtrip through a stdin reader.
              std::string out;
              auto proc = bld::run(bld::Cmd_loc{bld::Cmd{"cat"}}, bld::in_str{"hi\n"}, bld::io_out{&out});
              if (!proc) {
                  return std::unexpected(std::format("run cat with in_str failed: {}", proc.error()));
              }
              if (out != "hi\n") {
                  return std::unexpected(std::format("stdin content wrong: '{}'", out));
              }
#endif
              return {};
         }},
        {"capture_stdin_and_raw_crlf",         []() -> std::expected<void, std::string> {
              auto in = bld::capture(bld::Cmd_loc{true_cmd()}, bld::in_str{"hello"});
              if (!in) {
                  return std::unexpected(std::format("capture with in_str failed: {}", in.error()));
              }
              auto echo_in = bld::capture(bld::Cmd_loc{echo_cmd("out_data", "err_data")}, bld::in_str{"ignored"});
              if (!echo_in || echo_in->find("out_data") == std::string::npos) {
                  return std::unexpected("stdin pipe broke merged capture");
              }
#ifdef _WIN32
              bld::Cmd crlf{"cmd", "/c", "echo", "hi"};
#else
              bld::Cmd crlf{"printf", "hi\r\n"};
#endif
              auto norm = bld::capture(bld::Cmd_loc{crlf});
              auto raw = bld::capture(bld::Cmd_loc{crlf}, bld::raw_crlf{});
              if (!norm || *norm != "hi\n") {
                  return std::unexpected(std::format("CRLF normalization wrong: '{}'", norm ? *norm : "<err>"));
              }
              if (!raw || *raw != "hi\r\n") {
                  return std::unexpected(std::format("raw_crlf wrong: '{}'", raw ? *raw : "<err>"));
              }
              return {};
         }},
        {"keep_going_runs_past_failure",
         []() -> std::expected<void, std::string> {
              std::vector<bld::Task> tasks;
              tasks.emplace_back(bld::Cmd_loc{true_cmd()});
              tasks.emplace_back(bld::Cmd_loc{false_cmd()});
              tasks.emplace_back(bld::Cmd_loc{true_cmd()});
              auto res = bld::run(tasks, bld::jobs{1}, bld::keep_going{});
              if (res) {
                  return std::unexpected("keep_going batch unexpectedly succeeded");
              }
              // The run report now travels in the message: "2 ran, 0 skipped, 1 failed".
              if (res.error().msg.find("2 ran") == std::string::npos
                  || res.error().msg.find("1 failed") == std::string::npos) {
                  return std::unexpected(std::format("keep_going misreported: '{}'", res.error().msg));
              }
              return {};
         }},
        {"force_rebuilds_up_to_date_plan",
         []() -> std::expected<void, std::string> {
              if (!bld::fs::write_file("test_sandbox/force.out", "v1")) {
                  return std::unexpected("could not write force.out");
              }
              bld::Plan plan;
              plan.add("t", true_cmd());
              plan.produces("t", "test_sandbox/force.out");
              auto clean = bld::run(plan);
              if (!clean || clean->ran != 0 || clean->skipped != 1) {
                  return std::unexpected("up-to-date task should skip");
              }
              auto forced = bld::run(plan, bld::force{});
              if (!forced || forced->ran != 1) {
                  return std::unexpected("force should rebuild the skipped task");
              }
              return {};
         }},
        {"max_async_and_wait_all",
         []() -> std::expected<void, std::string> {
              std::vector<bld::Task> tasks;
              tasks.emplace_back(bld::Cmd_loc{sleep_cmd()});
              tasks.emplace_back(bld::Cmd_loc{sleep_cmd()});
              tasks.emplace_back(bld::Cmd_loc{sleep_cmd()});
              if (auto res = bld::run(tasks, bld::max_async{1}); !res) {
                  return std::unexpected(std::format("max_async run failed: {}", res.error()));
              }
              std::vector<bld::Proc> procs;
              for (int i = 0; i < 2; ++i) {
                  auto p = bld::run(bld::Cmd_loc{sleep_cmd()}, bld::async{});
                  if (!p) {
                      return std::unexpected(std::format("async spawn failed: {}", p.error()));
                  }
                  procs.push_back(std::move(*p));
              }
              auto waited = bld::wait_all(std::span<bld::Proc>{procs});
              if (!waited || *waited != 2) {
                  return std::unexpected("wait_all did not reap both procs");
              }
              return {};
         }},
        {"proc_group_wait_any_reaps_all",
         []() -> std::expected<void, std::string> {
             // wait_any blocks until any group child exits; each id resolves once.
             bld::Proc_group group;
             auto a = group.run_new(sleep_cmd());
             auto b = group.run_new(sleep_cmd());
             auto c = group.run_new(sleep_cmd());
             if (!a || !b || !c) {
                 return std::unexpected("group spawn failed");
             }
             std::size_t reaped = 0;
             while (!group.empty()) {
                 auto done = group.wait_any();
                 if (!done) {
                     return std::unexpected(std::format("wait_any failed: {}", done.error()));
                 }
                 if (!group.remove(*done)) {
                     return std::unexpected("wait_any returned an unknown id");
                 }
                 ++reaped;
             }
             if (reaped != 3) {
                 return std::unexpected(std::format("expected 3 reaps, got {}", reaped));
             }
             return {};
         }},
        {"span_tasks_build_graph",
         []() -> std::expected<void, std::string> {
             std::vector<bld::Task> tasks;
             bld::Task a{true_cmd()};
             a.name = "a";
             a.produces("test_sandbox/dedup.out");
             bld::Task b{true_cmd()};
             b.name = "b";
             b.needs("test_sandbox/dedup.out");
             tasks.push_back(std::move(a));
             tasks.push_back(std::move(b));
             auto report = bld::run(tasks, bld::jobs{2});
             if (!report || report->ran != 2) {
                 return std::unexpected("span tasks did not run the 2-task chain");
             }
             return {};
         }},
        {"span_deps_auto_graph",
         []() -> std::expected<void, std::string> {
             // Declared deps build the graph automatically; no flag needed.
             std::vector<bld::Task> tasks;
             bld::Task a{true_cmd()};
             a.name = "a";
             a.produces("test_sandbox/undep.out");
             bld::Task b{true_cmd()};
             b.name = "b";
             b.needs("test_sandbox/undep.out");
             tasks.push_back(std::move(a));
             tasks.push_back(std::move(b));
             auto report = bld::run(tasks, bld::jobs{2});
             if (!report || report->ran != 2) {
                 return std::unexpected("declared deps did not auto-build the graph");
             }
             return {};
         }},
        {"empty_command_names_task",
         []() -> std::expected<void, std::string> {
             std::vector<bld::Task> tasks;
             bld::Task t;
              t.name = "empty-cmd";
             tasks.push_back(std::move(t));
             auto report = bld::run(tasks);
             if (report) {
                 return std::unexpected("empty command unexpectedly succeeded");
             }
              if (report.error().msg.find("empty-cmd") == std::string::npos) {
                 return std::unexpected(std::format("error does not name task: '{}'", report.error().msg));
             }
             return {};
         }},
        {"self_dependency_rejected",
         []() -> std::expected<void, std::string> {
             std::vector<bld::Task> tasks;
             bld::Task a{true_cmd()};
             a.name = "self";
             a.after_dep("self");
             tasks.push_back(std::move(a));
             auto report = bld::run(tasks);
             if (report) {
                 return std::unexpected("self-dependency unexpectedly succeeded");
             }
             if (report.error().msg.find("itself") == std::string::npos) {
                 return std::unexpected(std::format("wrong error: '{}'", report.error().msg));
             }
             return {};
         }},
        {"dependency_cycle_rejected",
         []() -> std::expected<void, std::string> {
             std::vector<bld::Task> tasks;
             bld::Task a{true_cmd()};
             a.name = "a";
             a.produces("test_sandbox/cyc_a");
             a.needs("test_sandbox/cyc_b");
             bld::Task b{true_cmd()};
             b.name = "b";
             b.produces("test_sandbox/cyc_b");
             b.needs("test_sandbox/cyc_a");
             tasks.push_back(std::move(a));
             tasks.push_back(std::move(b));
             auto report = bld::run(tasks);
             if (report) {
                 return std::unexpected("cycle unexpectedly succeeded");
             }
             if (report.error().msg.find("cycle") == std::string::npos) {
                 return std::unexpected(std::format("wrong error: '{}'", report.error().msg));
             }
             return {};
         }},
        {"missing_cwd_rejected",
         []() -> std::expected<void, std::string> {
             auto proc = bld::run(true_cmd(), bld::cwd{"test_sandbox/nope"});
             if (proc) {
                 return std::unexpected("missing cwd unexpectedly succeeded");
             }
             if (proc.error().msg.find("does not exist") == std::string::npos) {
                 return std::unexpected(std::format("wrong error: '{}'", proc.error().msg));
             }
             return {};
         }},
        {"jobs_resolution",
         []() -> std::expected<void, std::string> {
              const std::size_t max = bld::max_parallel_count();
              if (max == 0) {
                  return std::unexpected("max_parallel_count is 0");
              }
              std::size_t def = max <= 1 ? 1 : max - 1;
              if (bld::resolve_parallel_width(std::nullopt) != def) {
                  return std::unexpected("default should be max - 1");
              }
              if (bld::resolve_parallel_width(0) != 1) {
                  return std::unexpected("0 should clamp to 1");
              }
              if (bld::resolve_parallel_width(1) != 1) {
                  return std::unexpected("1 should resolve to 1");
              }
              if (bld::resolve_parallel_width(1000000) != max) {
                  return std::unexpected("huge value should be capped by max");
              }
              if (bld::resolve_async_cap(0, 4) != 4) {
                  return std::unexpected("max_async 0 should follow parallel width");
              }
              if (bld::resolve_async_cap(3, 4) != 3) {
                  return std::unexpected("explicit max_async should pass through");
              }
              return {};
         }},
        {"log_indent_prefix_and_scope",
         []() -> std::expected<void, std::string> {
             std::ostringstream oss;
             auto *saved = bld::Logger::ostream.ptr;
             bld::Logger::ostream = oss;
             bld::log::set_indent(0);
             auto restore = [&]() {
                 bld::Logger::ostream.ptr = saved;
                 bld::log::set_indent(0);
             };
             bld::log::indent(2);
             if (bld::log::indent_level() != 2) {
                 restore();
                 return std::unexpected("indent(2) did not set level 2");
             }
             bld::log::i("hello");
             {
                 bld::log::indent_scope nest;
                 if (bld::log::indent_level() != 3) {
                     restore();
                     return std::unexpected("indent_scope did not nest to level 3");
                 }
                 bld::log::i("nested");
             }
             if (bld::log::indent_level() != 2) {
                 restore();
                 return std::unexpected("indent_scope did not restore level 2");
             }
             bld::log::unindent(99); // clamps at 0, never negative
             if (bld::log::indent_level() != 0) {
                 restore();
                 return std::unexpected("unindent did not clamp to 0");
             }
             bld::log::i("flat");
             restore();
             std::string out = oss.str();
             if (out.find("[INFO] :     hello\n") == std::string::npos) {
                 return std::unexpected(std::format("indent prefix wrong: '{}'", out));
             }
             if (out.find("[INFO] :       nested\n") == std::string::npos) {
                 return std::unexpected(std::format("nested indent prefix wrong: '{}'", out));
             }
             if (out.find("[INFO] : flat\n") == std::string::npos) {
                 return std::unexpected(std::format("restored indent prefix wrong: '{}'", out));
             }
             return {};
         }},
        {"single_dry_run_spawns_nothing",
         []() -> std::expected<void, std::string> {
             // false_cmd exits 1 when really run; dry-run must report success.
             auto proc = bld::run(bld::Cmd_loc{false_cmd()}, bld::dry_run{});
             if (!proc) {
                 return std::unexpected(std::format("dry-run single failed: {}", proc.error()));
             }
             if (proc->status_code() != 0 || proc->is_running()) {
                 return std::unexpected("dry-run proc should be exited/0 and not running");
             }
             auto ok = bld::run(bld::Cmd_loc{true_cmd()}, bld::dry_run{});
             if (!ok || ok->status_code() != 0) {
                 return std::unexpected("dry-run of true_cmd should succeed");
             }
             return {};
         }},
        {"capture_dry_run_returns_empty",
         []() -> std::expected<void, std::string> {
             // false_cmd fails when really run; dry-run must succeed empty.
             auto out = bld::capture(bld::Cmd_loc{false_cmd()}, bld::dry_run{});
             if (!out) {
                 return std::unexpected(std::format("dry-run capture failed: {}", out.error()));
             }
             if (!out->empty()) {
                 return std::unexpected(std::format("dry-run capture should be empty, got '{}'", *out));
             }
             return {};
         }},
        {"batch_dry_run_spawns_nothing",
         []() -> std::expected<void, std::string> {
             // false_cmd exits 1 when really run; batch dry-run must succeed
             // reporting everything skipped, spawning nothing.
             std::vector<bld::Task> tasks;
             tasks.emplace_back(bld::Cmd_loc{true_cmd()});
             tasks.emplace_back(bld::Cmd_loc{false_cmd()});
             auto res = bld::run(tasks, bld::jobs{2}, bld::dry_run{});
             if (!res) {
                 return std::unexpected(std::format("batch dry-run failed: {}", res.error()));
             }
             if (res->ran != 0 || res->skipped != 2 || !res->ok()) {
                 return std::unexpected(std::format("expected ran=0 skipped=2, got ran={} skipped={}", res->ran, res->skipped));
             }
             return {};
         }},
        {"scheduler_logs_task_failure",         []() -> std::expected<void, std::string> {
             std::ostringstream oss;
             auto *saved = bld::Logger::ostream.ptr;
             bld::Logger::ostream = oss;
             std::vector<bld::Task> tasks;
             tasks.emplace_back(bld::Cmd_loc{false_cmd()});
             tasks.back().name = "doomed";
             auto res = bld::run(tasks, bld::jobs{1});
             bld::Logger::ostream.ptr = saved;
             if (res) {
                 return std::unexpected("failing batch unexpectedly succeeded");
             }
             std::string logs = oss.str();
             if (logs.find("doomed") == std::string::npos) {
                 return std::unexpected(std::format("task name missing from logs: '{}'", logs));
             }
             if (logs.find("exited with status") == std::string::npos) {
                 return std::unexpected(std::format("exit status missing from logs: '{}'", logs));
             }
             if (logs.find("[100%] Task 'doomed' failed: exited with status 1") == std::string::npos) {
                 return std::unexpected(std::format("progress line wrong/missing: '{}'", logs));
             }
             return {};
         }},
        {"loader_infer_outputs",
         []() -> std::expected<void, std::string> {
             const std::string db = "test_sandbox/infer.json";
             auto w = bld::fs::write_file(
                 db, "[{\"directory\": \".\", \"file\": \"a.cpp\", \"arguments\": [\"g++\", \"-c\", \"a.cpp\", \"-o\", \"a.o\"]}]");
             if (!w) {
                 return std::unexpected("could not write test db");
             }
             auto with = bld::details::load_compile_commands(bld::compile_commands(db, true));
             auto without = bld::details::load_compile_commands(bld::compile_commands(db, false));
             if (!with || !without || with->empty() || without->empty()) {
                 return std::unexpected("loader failed");
             }
             if ((*with)[0].outputs.empty() || (*with)[0].outputs.front() != "a.o") {
                 return std::unexpected("infer_outputs=true did not parse -o");
             }
             if (!(*without)[0].outputs.empty()) {
                 return std::unexpected("infer_outputs=false should leave outputs empty");
             }
             if ((*with)[0].inputs.empty() || (*without)[0].inputs.empty()) {
                 return std::unexpected("loader should record the file as input");
             }
             return {};
         }},
        {"compile_commands_can_be_written_and_run_directly",
         []() -> std::expected<void, std::string> {
             const std::string database = "test_sandbox/compile_commands.json";
             bld::Plan plan;
             plan.add("unit", true_cmd());
             plan.needs("unit", "unit.cpp");
             plan.produces("unit", "unit.o");
             plan.mark_compile_command("unit");
             auto written = bld::run(plan, bld::force{}, bld::write_compile_commands{database});
             if (!written) {
                 return std::unexpected("failed to write compile_commands.json");
             }
              auto imported = bld::run(bld::compile_commands(database));
              if (!imported || imported->ran != 1) {
                  return std::unexpected("compile_commands.json was not executable as a run input");
              }
              return {};
          }},
         {"compile_commands_json_layout",
          []() -> std::expected<void, std::string> {
              const std::string database = "test_sandbox/layout.json";
              bld::Plan plan;
              plan.add("unit", true_cmd());
              plan.needs("unit", "unit.cpp");
              plan.produces("unit", "unit.o");
              plan.mark_compile_command("unit");
              plan.add("other", true_cmd());
              plan.needs("other", "other.cpp");
              plan.produces("other", "other.o");
              // NOTE: "other" is deliberately not marked.
              auto written = bld::run(plan, bld::force{}, bld::write_compile_commands{database});
              if (!written) {
                  return std::unexpected("failed to write layout.json");
              }
              auto text = bld::fs::read_file(database);
              if (!text) {
                  return std::unexpected("could not read layout.json back");
              }
              if (text->find("\"file\":\"unit.cpp\"") == std::string::npos) {
                  return std::unexpected(std::format("missing file field: '{}'", *text));
              }
              if (text->find("\"output\":\"unit.o\"") == std::string::npos) {
                  return std::unexpected(std::format("missing output field: '{}'", *text));
              }
              if (text->find("\"directory\":\".\"") == std::string::npos) {
                  return std::unexpected(std::format("missing directory field: '{}'", *text));
              }
              if (text->find("other.cpp") != std::string::npos) {
                  return std::unexpected("unmarked task leaked into the database");
              }
              return {};
          }},
         {"compile_commands_real_roundtrip",
          []() -> std::expected<void, std::string> {
              if (!bld::capture(bld::Cmd{"g++", "--version"})) {
                  return {}; // no compiler on PATH; nothing to prove here
              }
              if (!bld::fs::write_file("test_sandbox/cc_a.cpp", "int cc_answer() { return 42; }\n")) {
                  return std::unexpected("could not write cc_a.cpp");
              }
              if (!bld::fs::write_file(
                      "test_sandbox/cc_main.cpp", "int cc_answer();\nint main() { return cc_answer() == 42 ? 0 : 1; }\n")) {
                  return std::unexpected("could not write cc_main.cpp");
              }
              const std::string database = "test_sandbox/cc_roundtrip.json";
              bld::Plan plan;
              plan.add("cc-a", bld::Cmd{"g++", "-c", "test_sandbox/cc_a.cpp", "-o", "test_sandbox/cc_a.o"});
              plan.needs("cc-a", "test_sandbox/cc_a.cpp");
              plan.produces("cc-a", "test_sandbox/cc_a.o");
              plan.mark_compile_command("cc-a");
              plan.add("cc-main", bld::Cmd{"g++", "-c", "test_sandbox/cc_main.cpp", "-o", "test_sandbox/cc_main.o"});
              plan.needs("cc-main", "test_sandbox/cc_main.cpp");
              plan.produces("cc-main", "test_sandbox/cc_main.o");
              plan.mark_compile_command("cc-main");
              if (auto built = bld::run(plan, bld::force{}, bld::write_compile_commands{database}); !built) {
                  return std::unexpected(std::format("plan build failed: {}", built.error()));
              }
              if (!bld::fs::exists("test_sandbox/cc_a.o") || !bld::fs::exists("test_sandbox/cc_main.o")) {
                  return std::unexpected("plan did not produce the object files");
              }
              std::error_code ec;
              fs::remove("test_sandbox/cc_a.o", ec);
              fs::remove("test_sandbox/cc_main.o", ec);
              auto rebuilt = bld::run(bld::compile_commands(database), bld::jobs{2});
              if (!rebuilt || rebuilt->ran != 2) {
                  return std::unexpected("database run did not rebuild both objects");
              }
              if (!bld::fs::exists("test_sandbox/cc_a.o") || !bld::fs::exists("test_sandbox/cc_main.o")) {
                  return std::unexpected("database run did not produce the object files");
              }
              auto linked = bld::run(bld::Cmd{"g++", "test_sandbox/cc_a.o", "test_sandbox/cc_main.o", "-o", "test_sandbox/cc_app"});
              if (!linked || linked->status_code() != 0) {
                  return std::unexpected("linking rebuilt objects failed");
              }
              auto app = bld::run(bld::Cmd{"test_sandbox/cc_app"});
              if (!app || app->status_code() != 0) {
                  return std::unexpected("rebuilt app did not exit 0");
              }
              return {};
          }},
         {"compile_commands_command_string_form",
          []() -> std::expected<void, std::string> {
              const std::string database = "test_sandbox/cmd_form.json";
              std::string command;
              for (const auto &arg : true_cmd().args_) {
                  if (!command.empty()) {
                      command += ' ';
                  }
                  command += arg;
              }
              auto w = bld::fs::write_file(
                  database, std::format("[{{\"directory\": \".\", \"file\": \"f.cpp\", \"command\": \"{}\"}}]", command));
              if (!w) {
                  return std::unexpected("could not write test db");
              }
              auto loaded = bld::details::load_compile_commands(bld::compile_commands(database));
              if (!loaded || loaded->empty()) {
                  return std::unexpected("command-string entry failed to load");
              }
              if ((*loaded)[0].spec.cmd.args_ != true_cmd().args_) {
                  return std::unexpected("command string was not split into argv correctly");
              }
              auto res = bld::run(bld::compile_commands(database));
              if (!res || res->ran != 1) {
                  return std::unexpected("command-string database did not run");
              }
              return {};
          }},
         {"compile_commands_missing_file",
          []() -> std::expected<void, std::string> {
              auto res = bld::run(bld::compile_commands("test_sandbox/does-not-exist.json"));
              if (res) {
                  return std::unexpected("missing database unexpectedly ran");
              }
              return {};
          }},
         {"compile_commands_malformed",
          []() -> std::expected<void, std::string> {
              if (!bld::fs::write_file("test_sandbox/bad.json", "not json")) {
                  return std::unexpected("could not write test db");
              }
              auto bad = bld::details::load_compile_commands(bld::compile_commands("test_sandbox/bad.json"));
              if (bad || bad.error().msg.find("array") == std::string::npos) {
                  return std::unexpected("non-array JSON should fail naming the array");
              }
              if (!bld::fs::write_file("test_sandbox/nofile.json", "[{\"directory\": \".\", \"arguments\": [\"true\"]}]")) {
                  return std::unexpected("could not write test db");
              }
              auto nofile = bld::details::load_compile_commands(bld::compile_commands("test_sandbox/nofile.json"));
              if (nofile || nofile.error().msg.find("requires file") == std::string::npos) {
                  return std::unexpected("entry without file should fail naming the requirement");
              }
              return {};
          }},

         {"task_batch_staggered_scheduler",
         []() -> std::expected<void, std::string> {
             std::vector<bld::Task> execution_list;
             execution_list.emplace_back(bld::Cmd_loc{sleep_cmd()});
             execution_list.emplace_back(bld::Cmd_loc{sleep_cmd()});
             execution_list.emplace_back(bld::Cmd_loc{sleep_cmd()});

             auto batch_status = bld::run(execution_list, bld::jobs{2});
             if (!batch_status) {
                 return std::unexpected("parallel batch tracking cluster crashed");
             }
             return {};
         }},
        {"task_batch_poison_interruption", []() -> std::expected<void, std::string> {
             std::vector<bld::Task> broken_list;
             broken_list.emplace_back(bld::Cmd_loc{true_cmd()});
             broken_list.emplace_back(bld::Cmd_loc{false_cmd()});
             broken_list.emplace_back(bld::Cmd_loc{true_cmd()});

             // Force jobs{1} to guarantee sequential processing.
             // This ensures the 3rd task is NEVER scheduled because the 2nd task poisons the batch queue.
             auto batch_status = bld::run(broken_list, bld::jobs{1});
             if (batch_status) {
                 return std::unexpected("scheduler silently swallowed task failure");
             }

             // The run report now travels in the message: "ran, skipped, failed".
             if (batch_status.error().msg.find("1 failed") == std::string::npos) {
                 return std::unexpected(std::format("scheduler failure misreported: '{}'", batch_status.error().msg));
             }
             return {};
         }}};

    int exit_code = bld::test::run_suite(suite);

    fs::remove_all("./test_sandbox");
    return exit_code;
}

auto main(int argc, char *argv[]) -> int
{
    if (auto res = bld::rebuild_this_when_needed_ext(argc, argv); !res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }
    return run_tests();
}
