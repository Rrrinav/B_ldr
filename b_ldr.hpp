/*
  MIT
  Copyright Dec 2025, Rinav (github: rrrinav)

  Permission is hereby granted, free of charge,
  to any person obtaining a copy of this software and associated documentation files(the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
  sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions :

  The above copyright notice and this permission notice shall be included in all copies
  or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS",
  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
  inspired by nob.h by rexim/alexey/tsoding.
  github.com/tsoding/nob.h
*/

/*
    Usage:
        // In exactly one translation unit:
        #define B_LDR_IMPLEMENTATION
        #include "b_ldr.hpp"
 */

#ifndef B_LDR_HPP
#define B_LDR_HPP

#include <algorithm>
#include <any>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <filesystem>
#include <flat_map>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
#else
// POSIX / Linux
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// Public API map
//   1. Errors and formatters         bld::Err
//   2. Logging                       bld::Logger, bld::log
//   3. Commands and processes        bld::Cmd, bld::Proc
//   4. Execution                     bld::run, bld::capture, bld::Task
//   5. Rebuild helpers               bld::is_outdated, bld::rebuild_this_when_needed
//   6. Config                        bld::Config
//   7. Test helpers                  bld::test
//   8. Filesystem                    bld::fs

namespace bld {
struct Err
{
    using Error_pt = std::shared_ptr<Err>;
    std::error_code err;
    std::string msg{""};
    std::any payload{};
    Error_pt cause_{nullptr};

    static auto erc(std::errc code, std::string message = "") -> Err;
    static auto erno(int code, std::string message = "") -> Err;

    template <typename T>
    auto with_payload(T &&data) && -> Err;

    auto with_cause(Err root_cause) && -> Err;
    auto with_cause(Error_pt root_cause_ptr) && -> Err;
};
} // namespace bld

// clang-format off
template <>
struct std::formatter<bld::Err>
{
    enum class mode { plain, debug };
    mode fmt = mode::plain;
    constexpr auto parse(std::format_parse_context &ctx) -> std::format_parse_context::iterator;
    auto format(const bld::Err &err, std::format_context &ctx) const -> std::format_context::iterator;
};
// clang-format on

namespace bld::log::detail {
// A proxy for easier handling of streams.
// clang-format off
struct Stream_proxy
{
    std::ostream *ptr = &std::cerr;
    void operator=(std::ostream &os);
    std::ostream &get() const;
};
// clang-format on
} // namespace bld::log::detail

namespace bld {
using namespace std::string_view_literals;

struct Logger
{
    enum class Level { dbg = 0, inf = 1, wrn = 2, err = 3, ftl = 4 };

    struct Log_record
    {
        Level lvl;
        std::chrono::system_clock::time_point timestamp;
        std::string_view str;
    };

    using Logger_fn_t = std::function<void(std::ostream &, const Log_record &)>;
    inline static log::detail::Stream_proxy ostream;

    // clang-format off
    struct Default_logger_fn
    {
        inline static Level min_lvl = Level::dbg;
        inline static bool use_color = false;
        inline static constexpr std::string_view reset = "\x1b[0m";

        struct Style
        {
            std::string_view label;
            std::string_view color;
        };

        [[nodiscard]]
        static constexpr auto style(Level lvl) noexcept -> Style;

        auto operator()(std::ostream& stream, const Log_record& record) const -> void;
    };
    // clang-format on

    inline static std::atomic<Level> min_lvl{Level::inf};

    static auto set_logger_fn(Logger_fn_t fn, std::source_location loc = std::source_location::current()) -> void;

    inline static std::atomic<bool> logger_locked{false};
    inline static Logger_fn_t logger_fn = Default_logger_fn{};
    inline static std::mutex config_mtx;
};
} // namespace bld

namespace bld::log {

template <typename... Args>
inline void invoke_logger(bld::Logger::Level level, std::ostream &str, std::format_string<Args...> fmt, Args &&...args);

template <typename... Args>
void i(std::format_string<Args...> fmt, Args &&...args);

template <typename... Args>
void w(std::format_string<Args...> fmt, Args &&...args);

template <typename... Args>
void e(std::format_string<Args...> fmt, Args &&...args);

template <typename... Args>
void d(std::format_string<Args...> fmt, Args &&...args);

template <typename... Args>
void f(std::format_string<Args...> fmt, Args &&...args);

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void i(Os &str, std::format_string<Args...> fmt, Args &&...args);

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void w(Os &str, std::format_string<Args...> fmt, Args &&...args);

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void e(Os &str, std::format_string<Args...> fmt, Args &&...args);

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void d(Os &str, std::format_string<Args...> fmt, Args &&...args);

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void f(Os &str, std::format_string<Args...> fmt, Args &&...args);

} // namespace bld::log

namespace bld {
struct Cmd
{
    using value_type = std::vector<std::string>;
    using const_iterator = std::vector<std::string>::const_iterator;
    using iterator = std::vector<std::string>::iterator;
    using span_type = std::span<std::string>;

    // clang-format off
    std::vector<std::string> args_;

    Cmd() = default;
    
    template <typename... Ts> requires(std::convertible_to<Ts, std::string_view> && ...)
    explicit Cmd(Ts &&...ts);
    
    auto push(std::string_view s) -> void;
    
    template <typename... Ts>
    auto emplace_b(Ts &&...ts) -> std::string &;
    
    template <typename... Ts>
    auto emplace(const_iterator it, Ts &&...ts) -> std::string &;
    
    auto begin(this auto &self) noexcept;
    auto end(this auto &self) noexcept;
    auto size(this auto const &self) noexcept -> std::size_t;
    auto empty(this auto const &self) noexcept -> bool;
    auto span(this auto &self) noexcept;
    
    [[nodiscard]] auto argv() const -> std::vector<char*>;
    [[nodiscard]] auto str() const -> std::string;
    auto reset() -> void;
    // clang-format on
};
} // namespace bld

// clang-format off
template <>
struct std::formatter<bld::Cmd>
{
    enum class mode { plain, unquoted, debug };
    mode fmt = mode::plain;
    constexpr auto parse(std::format_parse_context &ctx) -> std::format_parse_context::iterator;
    auto format(const bld::Cmd &cmd, std::format_context &ctx) const -> std::format_context::iterator;
};
// clang-format on

namespace bld {

struct Proc
{
    enum class State : ::std::uint8_t { running, exited, signaled, stopped, continued };

    struct Status
    {
        State state{State::exited};
        ::std::uint8_t code{EXIT_SUCCESS};
    };

#ifdef _WIN32
    using P_id = void *;
    P_id id_{nullptr};
#else
    using P_id = pid_t;
    P_id id_{-1};
#endif
    Status status_{};
    std::string label{""};

    struct Io_routing
    {
        int in{STDIN_FILENO};
        int out{STDOUT_FILENO};
        int err{STDERR_FILENO};
        bool merge_err_to_out{false};
    };

    static inline constexpr Io_routing Default_route{STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, false};

    explicit Proc() = default;
    explicit Proc(P_id id, const std::string &label_ = "");

    ~Proc();

    Proc(const Proc &) = delete;
    Proc &operator=(const Proc &) = delete;

    Proc(Proc &&other) noexcept;
    Proc &operator=(Proc &&other) noexcept;

    [[nodiscard]]
    auto wait() -> std::expected<Status, bld::Err>;

    [[nodiscard]]
    auto try_wait() -> std::expected<Status, bld::Err>;

    auto kill(int sig = SIGTERM) -> void;

    // clang-format off
    [[nodiscard]] auto pid() const -> P_id;
    [[nodiscard]] auto status() const -> Status;
    [[nodiscard]] auto is_running() const -> bool;
    [[nodiscard]] auto status_code() const -> int;
    // clang-format on

    [[nodiscard]]
    static auto spawn(const Cmd &cmd, const std::string &label_ = "", const Io_routing &io = Default_route) -> std::expected<Proc, bld::Err>;

    [[nodiscard]]
    static auto wait_pid(P_id pid, int options = 0) -> std::expected<Status, bld::Err>;

    [[nodiscard]]
    static auto try_wait_pid(P_id pid) -> std::expected<Status, bld::Err>;

    static auto parse_status(int wstatus) -> Status;

private:
    auto update_status(int wstatus) -> void;
};
} // namespace bld

// clang-format off
template <>
struct std::formatter<bld::Proc::Status>
{
    enum class mode { plain, debug };
    mode fmt = mode::plain;

    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator;
    auto format(const bld::Proc::Status& s, std::format_context& ctx) const -> std::format_context::iterator;
};

template <>
struct std::formatter<bld::Proc>
{
    bool show_pid   = false;
    bool show_debug = false;

    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator;
    auto format(const bld::Proc& p, std::format_context& ctx) const -> std::format_context::iterator;
};
// clang-format on

namespace bld {
enum class Open_mode { read, write, append };

struct Fd_view
{
    using Native_t = int;
    static constexpr Native_t INVALID = -1;
    static constexpr Native_t DEFAULT_IN = STDIN_FILENO;
    static constexpr Native_t DEFAULT_OUT = STDOUT_FILENO;
    static constexpr Native_t DEFAULT_ERR = STDERR_FILENO;

    Native_t val{INVALID};

    constexpr Fd_view() = default;
    explicit constexpr Fd_view(Native_t v);

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool;
};

struct Owned_Fd
{
    Fd_view::Native_t handle_{Fd_view::INVALID};

    constexpr Owned_Fd() = default;
    explicit constexpr Owned_Fd(Fd_view::Native_t v);

    ~Owned_Fd();

    Owned_Fd(const Owned_Fd &) = delete;
    Owned_Fd &operator=(const Owned_Fd &) = delete;

    Owned_Fd(Owned_Fd &&other) noexcept;
    Owned_Fd &operator=(Owned_Fd &&other) noexcept;

    auto close() -> void;

    operator Fd_view() const;

    [[nodiscard]]
    static auto open(std::string_view path, Open_mode mode = Open_mode::read) -> std::expected<Owned_Fd, bld::Err>;
};

struct Proc_config
{
    std::string label{""};
    bool async{false};
    std::source_location loc{std::source_location::current()};
    Fd_view out{Fd_view::INVALID};
    Fd_view err{Fd_view::INVALID};
    Fd_view in{Fd_view::INVALID};
    bool merge_err_and_out{false};
};

struct Capture_config
{
    std::string label{""};
    std::source_location loc{std::source_location::current()};
    Fd_view in{Fd_view::DEFAULT_IN};
    std::string *out{nullptr};
    std::string *err{nullptr};
    bool merge_out_err{false};
    std::string_view in_str{""};
};

namespace details {
auto execute(const bld::Cmd &cmd, const Proc_config &cfg, std::source_location loc = std::source_location::current())
    -> std::expected<bld::Proc, bld::Err>;

auto capture_execute(const bld::Cmd &cmd, bld::Capture_config &cap_cfg, std::source_location loc = std::source_location::current())
    -> std::expected<bld::Proc::Status, bld::Err>;
} // namespace details

// clang-format off
struct async
{
    auto operator()(Proc_config &cfg) const -> void;
};
struct label
{
    std::string val{""};
    auto operator()(bld::Proc_config &cfg) const -> void;
    auto operator()(bld::Capture_config &cfg) const -> void;
};
struct pipe
{
    Fd_view out{Fd_view::DEFAULT_OUT}, in{Fd_view::DEFAULT_IN}, err{Fd_view::DEFAULT_ERR};
    bool merge_out_err{false};
    auto operator()(Proc_config& cfg) const -> void;
};
struct out_s
{
    Fd_view fd{Fd_view::INVALID};
    auto operator()(bld::Proc_config& cfg) const -> void;
};
struct err_s
{
    Fd_view fd{Fd_view::INVALID};
    auto operator()(bld::Proc_config& cfg) const -> void;
};
struct in_s
{
    Fd_view fd{Fd_view::INVALID};
    auto operator()(bld::Proc_config& cfg) const -> void;
    auto operator()(bld::Capture_config& cfg) const -> void;
};
struct out_f
{
    bld::Owned_Fd fd{};
    out_f(std::string_view path, bld::Open_mode mode = bld::Open_mode::write);
    auto operator()(bld::Proc_config& cfg) const -> void;
};
struct err_f
{
    bld::Owned_Fd fd{};
    err_f(std::string_view path, bld::Open_mode mode = bld::Open_mode::write);
    auto operator()(bld::Proc_config& cfg) const -> void;
};
struct in_f
{
    bld::Owned_Fd fd{};
    in_f(std::string_view path);
    auto operator()(bld::Proc_config& cfg) const -> void;
    auto operator()(bld::Capture_config& cfg) const -> void;
};
struct cap_out
{
    std::string* ptr{nullptr};
    explicit cap_out(std::string& s);
    auto operator()(Capture_config& cfg) const -> void;
};
struct cap_err
{
    std::string* ptr{nullptr};
    explicit cap_err(std::string& s);
    auto operator()(Capture_config& cfg) const -> void;
};
struct cap_merge
{
    std::string* ptr{nullptr};
    explicit cap_merge(std::string& s);
    auto operator()(Capture_config& cfg) const -> void;
};
struct in_str
{
    std::string_view val;
    explicit in_str(std::string_view s);
    auto operator()(Capture_config& cfg) const -> void;
};
// clang-format on

template <typename T>
constexpr bool is_out_mod_v =
    std::is_same_v<std::remove_cvref_t<T>, out_s> || std::is_same_v<std::remove_cvref_t<T>, out_f> || std::is_same_v<std::remove_cvref_t<T>, cap_out>;
template <typename T>
constexpr bool is_err_mod_v =
    std::is_same_v<std::remove_cvref_t<T>, err_s> || std::is_same_v<std::remove_cvref_t<T>, err_f> || std::is_same_v<std::remove_cvref_t<T>, cap_err>;
template <typename T>
constexpr bool is_in_mod_v =
    std::is_same_v<std::remove_cvref_t<T>, in_s> || std::is_same_v<std::remove_cvref_t<T>, in_f> || std::is_same_v<std::remove_cvref_t<T>, in_str>;
template <typename T>
constexpr bool is_batch_mod_v = std::is_same_v<std::remove_cvref_t<T>, pipe>;
template <typename T>
constexpr bool is_cap_out_mod_v = std::is_same_v<std::remove_cvref_t<T>, cap_out> || std::is_same_v<std::remove_cvref_t<T>, cap_merge>;
template <typename T>
constexpr bool is_cap_err_mod_v = std::is_same_v<std::remove_cvref_t<T>, cap_err> || std::is_same_v<std::remove_cvref_t<T>, cap_merge>;

template <typename T>
concept Config_modifier_c = requires(T &&modifier, Proc_config &cfg) { modifier(cfg); };

template <typename... Configs>
constexpr auto validate_run_configs() -> void;

struct Cmd_loc
{
    const Cmd &cmd;
    std::source_location loc;
    Cmd_loc(const Cmd &c, std::source_location l = std::source_location::current());
};

template <typename... Configs>
    requires(Config_modifier_c<Configs> && ...)
auto run(Cmd_loc cl, Configs &&...confs) -> std::expected<bld::Proc, bld::Err>;

template <typename T>
concept Capture_modifier_c = requires(T &&modifier, Capture_config &cfg) { modifier(cfg); };

template <typename... Configs>
constexpr auto validate_capture_configs() -> void;

template <typename... Configs>
    requires(Capture_modifier_c<Configs> && ...)
auto capture(Cmd_loc cl, Configs &&...confs) -> std::expected<bld::Proc::Status, bld::Err>;

auto wait_all(std::span<bld::Proc> procs) -> std::expected<std::size_t, bld::Err>;

struct Task
{
    bld::Cmd cmd;
    bld::Proc_config cfg;

    template <typename... Configs>
        requires(Config_modifier_c<Configs> && ...)
    explicit Task(Cmd_loc cl, Configs &&...confs);
};

auto run(std::span<bld::Task> tasks, std::size_t max_jobs = 0) -> std::expected<void, bld::Err>;
} // namespace bld

namespace bld {
[[nodiscard]]
auto is_outdated(std::string_view target, std::string_view source) -> bool;

template <std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
[[nodiscard]] auto is_outdated(std::string_view target, const Range &sources) -> bool;

auto get_current_cxx_compiler() -> std::string_view;

auto rebuild_this_when_needed(int argc, char **argv, std::string_view compiler = "", std::source_location loc = std::source_location::current())
    -> void;

auto rebuild_this_when_needed_ext(
    int argc,
    char **argv,
    std::vector<std::string> flags = {},
    std::string_view compiler = "",
    std::source_location loc = std::source_location::current()) -> void;

} // namespace bld

namespace bld {
class Config
{
public:
    enum val_t { Bool = 0, Int = 1, Double = 2, String = 3, String_arr = 4 };
    using value_type = std::variant<bool, int, double, std::string, std::vector<std::string>>;

    struct Option
    {
        val_t type;
        std::string description;
        value_type default_val;
        std::vector<std::string> choices{};
    };

    std::unordered_map<std::string_view, value_type> data{};
    std::flat_map<std::string_view, Option> options{};

    static auto get() -> Config &;
    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;
    Config(Config &&) = delete;
    Config &operator=(Config &&) = delete;

    auto add_option(std::string_view flag, val_t type, std::string_view desc, value_type def = false, std::vector<std::string> valid_choices = {})
        -> Config &;

    struct Proxy
    {
        const Config *cfg;
        std::string_view key;

        operator bool() const;
        operator std::string() const;
        operator int() const;
        operator double() const;
        operator std::vector<std::string>() const;
    };
    auto operator[](std::string_view key) const -> Proxy;
    auto print_help(std::string_view prog_name, std::string_view specific_opt = "") const -> void;
    auto parse(int argc, char *argv[]) -> void;
    template <typename T>
    auto get_val(std::string_view key) const -> std::expected<T, bld::Err>;

private:
    Config() = default;
};
} // namespace bld

template <>
struct std::formatter<std::unordered_map<std::string_view, bld::Config::value_type>>
{
    constexpr auto parse(std::format_parse_context &ctx) -> std::format_parse_context::iterator;
    auto format(const std::unordered_map<std::string_view, bld::Config::value_type> &m, std::format_context &ctx) const
        -> std::format_context::iterator;
};

namespace bld::test {

// clang-format off
enum class Edit_type : std::uint8_t { keep, insert, remove };
struct Edit
{
    Edit_type type;
    std::string_view content;
};
struct Frontier
{
    std::vector<std::ptrdiff_t> data;
    std::ptrdiff_t offset;
    explicit Frontier(std::ptrdiff_t max_d);
    auto operator[](std::ptrdiff_t k) -> std::ptrdiff_t &;
    auto operator[](std::ptrdiff_t k) const -> std::ptrdiff_t;
};
struct compute_diff_op
{
    bool same{false};
    std::vector<Edit> edits;
    operator bool() const;
};
// clang-format on

auto compute_diff(std::span<const std::string_view> original, std::span<const std::string_view> updated) -> compute_diff_op;
auto split_lines(std::string_view text) -> std::vector<std::string_view>;
auto compute_diff(std::initializer_list<std::string_view> original, std::initializer_list<std::string_view> updated) -> compute_diff_op;
auto compute_diff(std::string_view original, std::string_view updated) -> compute_diff_op;
} // namespace bld::test

template <>
struct std::formatter<bld::test::compute_diff_op>
{
    bool use_color = true;

    constexpr auto parse(std::format_parse_context &ctx) -> std::format_parse_context::iterator;
    auto format(const bld::test::compute_diff_op &diff, std::format_context &ctx) const -> std::format_context::iterator;
};

namespace bld::fs {
auto make_dir_if_not_exists(std::string_view path, bool create_parents = true, std::source_location loc = std::source_location::current()) noexcept
    -> bool;

struct Dir_entry
{
    std::filesystem::path path;
    std::filesystem::file_type type;
    int depth{0};

    [[nodiscard]] bool is_file() const noexcept;
    [[nodiscard]] bool is_dir() const noexcept;
    [[nodiscard]] bool is_symlink() const noexcept;
    [[nodiscard]] bool is_hidden() const noexcept;

    [[nodiscard]] std::string_view extension() const noexcept;
    [[nodiscard]] std::string_view stem() const noexcept;
    [[nodiscard]] std::string_view filename() const noexcept;
    [[nodiscard]] std::string_view parent() const noexcept;
};

enum class Walk_action { next, skip_dir, stop };

struct Walk_result
{
    Walk_action action = Walk_action::next;
    std::error_code error = {};

    constexpr Walk_result() = default;
    constexpr Walk_result(Walk_action a) noexcept;
    constexpr Walk_result(std::error_code ec) noexcept;
};

struct Walk_error
{
    enum class Kind { fs, visitor } kind;
    std::error_code code;

    [[nodiscard]] bool is_fs_error() const noexcept;
    [[nodiscard]] bool is_visitor_error() const noexcept;
    [[nodiscard]] std::string message() const;
};

template <typename T>
using Walk_result_t = std::expected<T, Walk_error>;

namespace detail {
template <typename V>
concept Void_visitor = std::invocable<V, const Dir_entry &> && std::same_as<std::invoke_result_t<V, const Dir_entry &>, void>;
template <typename V>
concept Result_visitor = std::invocable<V, const Dir_entry &> && std::convertible_to<std::invoke_result_t<V, const Dir_entry &>, Walk_result>;
template <typename V>
concept Valid_visitor = Void_visitor<V> || Result_visitor<V>;

template <Valid_visitor V>
auto invoke_visitor(V &&v, const Dir_entry &e) -> Walk_result;
} // namespace detail

class Dir_walker
{
public:
    explicit Dir_walker(std::string_view root);
    explicit Dir_walker(std::filesystem::path root);
    explicit Dir_walker(const std::string &root);

    [[nodiscard]] auto recursive(bool v = true) noexcept -> Dir_walker &;
    [[nodiscard]] auto flat() noexcept -> Dir_walker &;
    [[nodiscard]] auto include_dirs(bool v = true) noexcept -> Dir_walker &;
    [[nodiscard]] auto include_hidden(bool v = true) noexcept -> Dir_walker &;
    [[nodiscard]] auto follow_symlinks(bool v = true) noexcept -> Dir_walker &;
    [[nodiscard]] auto max_depth(int v) noexcept -> Dir_walker &;

    [[nodiscard]] auto ext(std::string e) -> Dir_walker &;
    [[nodiscard]] auto ext(std::initializer_list<std::string_view> exts) -> Dir_walker &;
    [[nodiscard]] auto named(std::string name) -> Dir_walker &;
    [[nodiscard]] auto skip(std::string dir_name) -> Dir_walker &;
    [[nodiscard]] auto skip(std::initializer_list<std::string_view> dir_names) -> Dir_walker &;

    template <typename Pred>
        requires std::predicate<Pred, const Dir_entry &>
    [[nodiscard]] auto where(Pred pred) -> Dir_walker &;
    template <detail::Valid_visitor V>
    [[nodiscard]] auto walk(V &&visitor, std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<void>;
    [[nodiscard]] auto collect(std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<std::vector<Dir_entry>>;
    [[nodiscard]]
    auto collect_paths(std::source_location caller = std::source_location::current()) const noexcept
        -> Walk_result_t<std::vector<std::filesystem::path>>;
    [[nodiscard]]
    auto count(std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<std::size_t>;
    [[nodiscard]]
    auto any(std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<bool>;
    [[nodiscard]]
    auto none(std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<bool>;
    [[nodiscard]]
    auto first(std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<std::optional<Dir_entry>>;
    [[nodiscard]]
    auto last(std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<std::optional<Dir_entry>>;
    template <std::invocable<const Dir_entry &> F>
    [[nodiscard]]
    auto for_each(F &&fn, std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<void>;
    template <typename Pred>
        requires std::predicate<Pred, const Dir_entry &>
    [[nodiscard]]
    auto partition(Pred pred, std::source_location caller = std::source_location::current()) const noexcept
        -> Walk_result_t<std::pair<std::vector<Dir_entry>, std::vector<Dir_entry>>>;
    template <typename T, std::invocable<T, const Dir_entry &> F>
    [[nodiscard]]
    auto fold(T init, F &&fn, std::source_location caller = std::source_location::current()) const noexcept -> Walk_result_t<T>;
    [[nodiscard]]
    auto subdir(std::string_view sub) const -> Dir_walker;
    template <std::invocable<Dir_walker &> F>
    [[nodiscard]] auto apply(F &&fn) -> Dir_walker &;

private:
    std::filesystem::path root_;
    bool recursive_ = true;
    bool include_dirs_ = false;
    bool include_hidden_ = false;
    bool follow_symlinks_ = false;
    int max_depth_ = std::numeric_limits<int>::max();
    std::vector<std::function<bool(const Dir_entry &)>> filters_;
    std::vector<std::string> skips_;

    [[nodiscard]] bool passes_filters(const Dir_entry &e) const;
    [[nodiscard]] bool is_skipped(const Dir_entry &e) const;
    [[nodiscard]] bool is_visible(const Dir_entry &e) const;
    static auto make_entry(const std::filesystem::directory_entry &raw, int d) -> Dir_entry;

    template <std::invocable<const Dir_entry &> F>
    [[nodiscard]]
    auto run(F &&fn, std::source_location caller) const noexcept -> Walk_result_t<void>;
};

template <detail::Valid_visitor V>
[[nodiscard]] inline auto walk(std::string_view root, V &&visitor, std::source_location caller = std::source_location::current()) noexcept;

[[nodiscard]] auto collect(std::string_view root, std::source_location caller = std::source_location::current()) noexcept
    -> Walk_result_t<std::vector<Dir_entry>>;

// "src/main.cpp" -> "main"
[[nodiscard]] auto stem(std::string_view path) noexcept -> std::string;
// "src/main.cpp" -> "main.cpp"
[[nodiscard]] auto name(std::string_view path) noexcept -> std::string;
// "src/main.cpp" -> ".cpp"
[[nodiscard]] auto extension(std::string_view path) noexcept -> std::string;
// "src/main.cpp" -> "src"
[[nodiscard]] auto parent_dir(std::string_view path) noexcept -> std::string;
[[nodiscard]] auto is_absolute(std::string_view path) noexcept -> bool;
[[nodiscard]] auto is_relative(std::string_view path) noexcept -> bool;

[[nodiscard]] auto exists(std::string_view path) noexcept -> bool;
[[nodiscard]] auto is_dir(std::string_view path) noexcept -> bool;
[[nodiscard]] auto is_file(std::string_view path) noexcept -> bool;
[[nodiscard]] auto is_symlink(std::string_view path) noexcept -> bool;
[[nodiscard]] auto is_empty(std::string_view path) noexcept -> std::expected<bool, bld::Err>;

template <typename... Paths>
    requires(std::convertible_to<Paths, std::string_view> && ...)
[[nodiscard]] auto join(Paths &&...paths) -> std::string;

auto file_size(std::string_view path) noexcept -> std::expected<std::uintmax_t, bld::Err>;
auto last_write_time(std::string_view path) noexcept -> std::expected<std::filesystem::file_time_type, bld::Err>;

auto copy_file(std::string_view from, std::string_view to, bool overwrite = false) noexcept -> std::expected<void, bld::Err>;
auto rename(std::string_view from, std::string_view to) noexcept -> std::expected<void, bld::Err>;

auto create_symlink(std::string_view target, std::string_view link) noexcept -> std::expected<void, bld::Err>;
auto create_hard_link(std::string_view target, std::string_view link) noexcept -> std::expected<void, bld::Err>;
auto read_symlink(std::string_view path) noexcept -> std::expected<std::string, bld::Err>;

auto current_path() noexcept -> std::expected<std::string, bld::Err>;
auto set_current_path(std::string_view path) noexcept -> std::expected<void, bld::Err>;
auto absolute(std::string_view path) noexcept -> std::expected<std::string, bld::Err>;
auto canonical(std::string_view path) noexcept -> std::expected<std::string, bld::Err>;
auto relative(std::string_view path, std::string_view base) noexcept -> std::expected<std::string, bld::Err>;

auto read_file(std::string_view path) noexcept -> std::expected<std::string, bld::Err>;
auto write_file(std::string_view path, std::string_view content) noexcept -> std::expected<void, bld::Err>;
auto append_file(std::string_view path, std::string_view content) noexcept -> std::expected<void, bld::Err>;

template <typename... Paths>
    requires(std::convertible_to<Paths, std::string_view> && ...)
auto remove(Paths &&...paths) noexcept -> std::expected<void, bld::Err>;

template <typename... Paths>
    requires(std::convertible_to<Paths, std::string_view> && ...)
auto make_dirs(Paths &&...paths) noexcept -> std::expected<void, bld::Err>;

inline auto find_all_files(std::string_view root) noexcept -> Walk_result_t<std::vector<std::string>>;

template <typename... Exts>
    requires(std::convertible_to<Exts, std::string_view> && ...)
auto find_by_ext(std::string_view root, Exts &&...exts) noexcept -> Walk_result_t<std::vector<std::string>>;

template <typename... Names>
    requires(std::convertible_to<Names, std::string_view> && ...)
auto find_by_name(std::string_view root, Names &&...names) noexcept -> Walk_result_t<std::vector<std::string>>;

} // namespace bld::fs

namespace bld::str {
[[nodiscard]] auto trim_left(std::string_view s) noexcept -> std::string_view;
[[nodiscard]] auto trim_right(std::string_view s) noexcept -> std::string_view;
[[nodiscard]] auto trim(std::string_view s) noexcept -> std::string_view;
[[nodiscard]] auto split(std::string_view s, char delimiter) -> std::vector<std::string_view>;
[[nodiscard]] auto split(std::string_view s, std::string_view delimiter) -> std::vector<std::string_view>;
[[nodiscard]] auto to_lower(std::string_view s) -> std::string;
[[nodiscard]] auto to_upper(std::string_view s) -> std::string;
[[nodiscard]] auto replace_all(std::string_view s, std::string_view from, std::string_view to) -> std::string;
[[nodiscard]] auto parse_int(std::string_view s, int base = 10) noexcept -> std::expected<int, bld::Err>;
[[nodiscard]] auto parse_double(std::string_view s) noexcept -> std::expected<double, bld::Err>;
[[nodiscard]] auto parse_bool(std::string_view s) noexcept -> std::expected<bool, bld::Err>;
template <std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
[[nodiscard]] auto join(const Range &range, std::string_view delimiter) -> std::string;
} // namespace bld::str

namespace bld::time {
struct stamp
{
    using clock_t = std::chrono::steady_clock;
    using time_point_t = clock_t::time_point;
    time_point_t tp_{clock_t::now()};
    stamp() = default;
    auto reset() noexcept -> std::chrono::nanoseconds;
    [[nodiscard]] auto elapsed() const noexcept -> std::chrono::nanoseconds;
    [[nodiscard]] auto since(const stamp &baseline) const noexcept -> std::chrono::nanoseconds;
};

[[nodiscard]] auto now() noexcept -> stamp;
[[nodiscard]] auto since(const stamp &baseline) noexcept -> std::chrono::nanoseconds;
[[nodiscard]] auto format(std::chrono::nanoseconds ns) -> std::string;
}; // namespace bld::time

namespace std {
constexpr auto formatter<bld::Err>::parse(format_parse_context &ctx) -> format_parse_context::iterator
{
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
        switch (*it) {
        case '?':
            fmt = mode::debug;
            break;
        case 'p':
            fmt = mode::plain;
            break;
        default:
            throw format_error("invalid Cmd format");
        }
        ++it;
    }
    return it;
}

constexpr auto formatter<bld::Cmd>::parse(format_parse_context &ctx) -> format_parse_context::iterator
{
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
        switch (*it) {
        case 'q':
            fmt = mode::unquoted;
            break;
        case '?':
            fmt = mode::debug;
            break;
        case 'p':
            fmt = mode::plain;
            break;
        default:
            throw format_error("invalid Cmd format");
        }
        ++it;
    }
    return it;
}

constexpr auto formatter<bld::Proc::Status>::parse(format_parse_context &ctx) -> format_parse_context::iterator
{
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
        switch (*it) {
        case 'p':
            fmt = mode::plain;
            break;
        case '?':
            fmt = mode::debug;
            break;
        default:
            throw format_error("invalid Proc::Status format specifier");
        }
        ++it;
    }
    return it;
}

constexpr auto formatter<bld::Proc>::parse(format_parse_context &ctx) -> format_parse_context::iterator
{
    auto it = ctx.begin();
    while (it != ctx.end() && *it != '}') {
        switch (*it) {
        case 'p':
            show_pid = true;
            break;
        case '?':
            show_debug = true;
            break;
        default:
            throw format_error("invalid Proc format specifier");
        }
        ++it;
    }
    return it;
}

constexpr auto formatter<unordered_map<string_view, bld::Config::value_type>>::parse(format_parse_context &ctx) -> format_parse_context::iterator
{
    auto it = ctx.begin();
    return it;
}

constexpr auto formatter<bld::test::compute_diff_op>::parse(format_parse_context &ctx) -> format_parse_context::iterator
{
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it != end && *it != '}') {
        if (*it == 'n') {
            use_color = false;
            ++it;
        } else if (*it == 'c') {
            use_color = true;
            ++it;
        } else {
            throw format_error("invalid format specifier for compute_diff_op");
        }
    }
    if (it != end && *it != '}') {
        throw format_error("invalid format specifier for compute_diff_op");
    }
    return it;
}

} // namespace std

namespace bld {

template <typename T>
auto Err::with_payload(T &&data) && -> Err
{
    payload = std::forward<T>(data);
    return std::move(*this);
}

constexpr auto Logger::Default_logger_fn::style(Level lvl) noexcept -> Style
{
    using namespace std::string_view_literals;
    switch (lvl) {
    case Level::dbg:
        return {"[DEBUG]", "\x1b[38;2;120;170;255m"sv};
    case Level::inf:
        return {"[INFO] ", "\x1b[38;2;0;200;120m"sv};
    case Level::wrn:
        return {"[WARN] ", "\x1b[38;2;255;180;0m"sv};
    case Level::err:
        return {"[ERROR]", "\x1b[38;2;255;64;64m"sv};
    case Level::ftl:
        return {"[FATAL]", "\x1b[38;2;200;0;0m"sv};
    }
    std::unreachable();
}

template <typename... Ts>
    requires(std::convertible_to<Ts, std::string_view> && ...)
Cmd::Cmd(Ts &&...ts)
{
    args_.reserve(sizeof...(Ts));
    (args_.emplace_back(std::forward<Ts>(ts)), ...);
}

template <typename... Ts>
auto Cmd::emplace_b(Ts &&...ts) -> std::string &
{
    return args_.emplace_back(std::forward<Ts>(ts)...);
}

template <typename... Ts>
auto Cmd::emplace(const_iterator it, Ts &&...ts) -> std::string &
{
    return args_.emplace(it, std::forward<Ts>(ts)...);
}

auto Cmd::begin(this auto &self) noexcept
{
    return self.args_.begin();
}
auto Cmd::end(this auto &self) noexcept
{
    return self.args_.end();
}
auto Cmd::size(this auto const &self) noexcept -> std::size_t
{
    return self.args_.size();
}
auto Cmd::empty(this auto const &self) noexcept -> bool
{
    return self.args_.empty();
}
auto Cmd::span(this auto &self) noexcept
{
    return std::span{self.args_};
}

constexpr Fd_view::Fd_view(Native_t v) : val(v)
{}
constexpr auto Fd_view::is_valid() const noexcept -> bool
{
    return val != INVALID;
}

constexpr Owned_Fd::Owned_Fd(Fd_view::Native_t v) : handle_(v)
{}

template <typename... Configs>
constexpr auto validate_run_configs() -> void
{
    constexpr int out_count = (bld::is_out_mod_v<Configs> + ... + 0);
    constexpr int err_count = (bld::is_err_mod_v<Configs> + ... + 0);
    constexpr int in_count = (bld::is_in_mod_v<Configs> + ... + 0);
    constexpr int batch_count = (bld::is_batch_mod_v<Configs> + ... + 0);
    static_assert(out_count <= 1, "API ERROR: Duplicate 'out pipe' modifiers. You passed multiple `bld::out_f` / `bld::out_s`.");
    static_assert(err_count <= 1, "API ERROR: Duplicate 'err pipe' modifiers. You passed multiple `bld::err_f` / `bld::err_s`.");
    static_assert(in_count <= 1, "API ERROR: Duplicate 'in pipe' modifiers. You passed multiple `bld::in_f` / `bld::in_s`.");
    static_assert(batch_count <= 1, "API ERROR: Duplicate `bld::pipe` batch modifiers passed.");
    static_assert(
        batch_count == 0 || (out_count == 0 && err_count == 0 && in_count == 0),
        "FATAL API ERROR: Cannot mix monolithic `bld::pipe` with granular `bld::out_f`, `bld::err_s`, etc.");
}

template <typename... Configs>
    requires(Config_modifier_c<Configs> && ...)
auto run(Cmd_loc cl, Configs &&...confs) -> std::expected<bld::Proc, bld::Err>
{
    bld::validate_run_configs<Configs...>();

    Proc_config cfg{};
    (confs(cfg), ...);
    if (cfg.loc.line() == std::source_location::current().line()) {
        cfg.loc = cl.loc;
    }
    return bld::details::execute(cl.cmd, cfg, cl.loc);
}

template <typename... Configs>
constexpr auto validate_capture_configs() -> void
{
    constexpr int out_count = (bld::is_cap_out_mod_v<Configs> + ... + 0);
    constexpr int err_count = (bld::is_cap_err_mod_v<Configs> + ... + 0);
    static_assert(out_count <= 1 && err_count <= 1, "FATAL: Conflicting capture modifiers passed.");
}

template <typename... Configs>
    requires(Capture_modifier_c<Configs> && ...)
auto capture(Cmd_loc cl, Configs &&...confs) -> std::expected<bld::Proc::Status, bld::Err>
{
    bld::validate_capture_configs<Configs...>();

    Capture_config cap_cfg{};
    (confs(cap_cfg), ...);
    if (cap_cfg.loc.line() == std::source_location::current().line()) {
        cap_cfg.loc = cl.loc;
    }

    return bld::details::capture_execute(cl.cmd, cap_cfg, cl.loc);
}

template <typename... Configs>
    requires(Config_modifier_c<Configs> && ...)
Task::Task(Cmd_loc cl, Configs &&...confs) : cmd(cl.cmd)
{
    bld::validate_run_configs<Configs...>();
    (confs(cfg), ...);
    cfg.loc = cl.loc;
    cfg.async = true;
}

template <std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
auto is_outdated(std::string_view target, const Range &sources) -> bool
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(target, ec)) {
        bld::log::w("Target '{}' does not exist. Rebuild required.", target);
        return true;
    }
    auto target_time = fs::last_write_time(target, ec);
    if (ec) {
        bld::log::w("Failed to read timestamp for target '{}': {}. Defaulting to rebuild.", target, ec.message());
        return true;
    }
    for (const auto &src : sources) {
        std::string_view source{src};
        if (!fs::exists(source, ec)) {
            bld::log::w("Source file missing: '{}'. Forcing rebuild.", source);
            return true;
        }
        auto source_time = fs::last_write_time(source, ec);
        if (ec) {
            bld::log::w("Failed to read timestamp for source '{}': {}. Defaulting to rebuild.", source, ec.message());
            return true;
        }
        if (target_time < source_time) {
            return true;
        }
    }
    return false;
}

template <typename T>
auto Config::get_val(std::string_view key) const -> std::expected<T, bld::Err>
{
    auto it = data.find(key);
    if (it == data.end()) {
        return std::unexpected(bld::Err::erc(std::errc::invalid_argument, std::format("Configuration key '{}' not found", key)));
    }
    if (auto *p = std::get_if<T>(&it->second)) {
        return *p;
    }
    return std::unexpected(bld::Err::erc(std::errc::invalid_argument, std::format("Configuration key '{}' has mismatched type", key)));
}

} // namespace bld

namespace bld::log {

template <typename... Args>
inline void invoke_logger(bld::Logger::Level level, std::ostream &str, std::format_string<Args...> fmt, Args &&...args)
{
    if (level < bld::Logger::min_lvl.load(std::memory_order_relaxed)) {
        return;
    }
    std::string formatted_str = std::format(fmt, std::forward<Args>(args)...);
    bld::Logger::Log_record record{.lvl = level, .timestamp = std::chrono::system_clock::now(), .str = formatted_str};
    std::invoke(bld::Logger::logger_fn, str, record);
}

template <typename... Args>
void i(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::inf, bld::Logger::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void w(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::wrn, bld::Logger::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void e(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::err, bld::Logger::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void d(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::dbg, bld::Logger::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void f(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::ftl, bld::Logger::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void i(Os &str, std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::inf, str, fmt, std::forward<Args>(args)...);
}

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void w(Os &str, std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::wrn, str, fmt, std::forward<Args>(args)...);
}

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void e(Os &str, std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::err, str, fmt, std::forward<Args>(args)...);
}

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void d(Os &str, std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::dbg, str, fmt, std::forward<Args>(args)...);
}

template <typename Os, typename... Args>
    requires std::derived_from<std::remove_reference_t<Os>, std::ostream>
void f(Os &str, std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::ftl, str, fmt, std::forward<Args>(args)...);
}

} // namespace bld::log

namespace bld::fs {

constexpr Walk_result::Walk_result(Walk_action a) noexcept : action{a}
{}
constexpr Walk_result::Walk_result(std::error_code ec) noexcept : action{ec ? Walk_action::stop : Walk_action::next}, error{ec}
{}

namespace detail {
template <Valid_visitor V>
auto invoke_visitor(V &&v, const Dir_entry &e) -> Walk_result
{
    if constexpr (Void_visitor<V>) {
        std::invoke(std::forward<V>(v), e);
        return {};
    } else {
        return std::invoke(std::forward<V>(v), e);
    }
}
} // namespace detail

template <typename Pred>
    requires std::predicate<Pred, const Dir_entry &>
auto Dir_walker::where(Pred pred) -> Dir_walker &
{
    filters_.emplace_back(std::move(pred));
    return *this;
}

template <detail::Valid_visitor V>
auto Dir_walker::walk(V &&visitor, std::source_location caller) const noexcept -> Walk_result_t<void>
{
    return run([&](const Dir_entry &e) { return detail::invoke_visitor(std::forward<V>(visitor), e); }, caller);
}

template <std::invocable<const Dir_entry &> F>
auto Dir_walker::for_each(F &&fn, std::source_location caller) const noexcept -> Walk_result_t<void>
{
    return run(
        [&](const Dir_entry &e) -> Walk_result {
            std::invoke(std::forward<F>(fn), e);
            return Walk_action::next;
        },
        caller);
}

template <typename Pred>
    requires std::predicate<Pred, const Dir_entry &>
auto Dir_walker::partition(Pred pred, std::source_location caller) const noexcept
    -> Walk_result_t<std::pair<std::vector<Dir_entry>, std::vector<Dir_entry>>>
{
    std::vector<Dir_entry> yes, no;
    return run(
               [&](const Dir_entry &e) -> Walk_result {
                   (std::invoke(pred, e) ? yes : no).push_back(e);
                   return Walk_action::next;
               },
               caller)
        .transform([&] { return std::pair{std::move(yes), std::move(no)}; });
}

template <typename T, std::invocable<T, const Dir_entry &> F>
auto Dir_walker::fold(T init, F &&fn, std::source_location caller) const noexcept -> Walk_result_t<T>
{
    T acc = std::move(init);
    return run(
               [&](const Dir_entry &e) -> Walk_result {
                   acc = std::invoke(std::forward<F>(fn), std::move(acc), e);
                   return Walk_action::next;
               },
               caller)
        .transform([&] { return std::move(acc); });
}

template <std::invocable<Dir_walker &> F>
auto Dir_walker::apply(F &&fn) -> Dir_walker &
{
    std::invoke(std::forward<F>(fn), *this);
    return *this;
}

template <std::invocable<const Dir_entry &> F>
auto Dir_walker::run(F &&fn, std::source_location caller) const noexcept -> Walk_result_t<void>
{
    namespace fs = std::filesystem;

    if (root_.empty()) {
        bld::log::w("Dir_walker: empty root ({})", caller.function_name());
        return std::unexpected{Walk_error{.kind = Walk_error::Kind::fs, .code = std::make_error_code(std::errc::invalid_argument)}};
    }

    std::error_code ec;
    const auto iter_opts = follow_symlinks_ ? fs::directory_options::skip_permission_denied | fs::directory_options::follow_directory_symlink
                                            : fs::directory_options::skip_permission_denied;

    auto handle = [&](const Dir_entry &e) -> Walk_result {
        if (is_skipped(e)) {
            return Walk_action::skip_dir;
        }
        if (!is_visible(e)) {
            return Walk_action::next;
        }
        if (!passes_filters(e)) {
            return Walk_action::next;
        }
        return std::invoke(fn, e);
    };

    if (!recursive_) {
        fs::directory_iterator it{root_, iter_opts, ec};
        if (ec) {
            return std::unexpected{Walk_error{.kind = Walk_error::Kind::fs, .code = ec}};
        }

        for (const auto &raw : it) {
            const auto [action, err] = handle(make_entry(raw, 0));
            if (err) {
                return std::unexpected{Walk_error{.kind = Walk_error::Kind::visitor, .code = err}};
            }
            if (action == Walk_action::stop) {
                return {};
            }
        }
        return {};
    }

    fs::recursive_directory_iterator it{root_, iter_opts, ec};
    if (ec) {
        return std::unexpected{Walk_error{.kind = Walk_error::Kind::fs, .code = ec}};
    }

    for (const auto &raw : it) {
        const int d = it.depth();
        if (d >= max_depth_) {
            it.disable_recursion_pending();
        }

        const auto [action, err] = handle(make_entry(raw, d));

        if (err) {
            return std::unexpected{Walk_error{.kind = Walk_error::Kind::visitor, .code = err}};
        }

        switch (action) {
        case Walk_action::stop:
            it = {};
            return {};
        case Walk_action::skip_dir:
            it.disable_recursion_pending();
            break;
        case Walk_action::next:
            break;
        }
    }

    return {};
}

template <detail::Valid_visitor V>
inline auto walk(std::string_view root, V &&visitor, std::source_location caller) noexcept
{
    return Dir_walker{root}.walk(std::forward<V>(visitor), caller);
}

} // namespace bld::fs

#endif // B_LDR_HPP

#ifdef B_LDR_IMPLEMENTATION
#ifndef B_LDR_IMPLEMENTATION_ONCE
#define B_LDR_IMPLEMENTATION_ONCE

// Implementation

#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

auto bld::Err::erc(std::errc code, std::string message) -> Err
{
    return Err{.err = std::make_error_code(code), .msg = std::move(message)};
}

auto bld::Err::erno(int code, std::string message) -> Err
{
    return Err{.err = std::error_code(code, std::generic_category()), .msg = std::move(message)};
}

auto bld::Err::with_cause(Err root_cause) && -> Err
{
    cause_ = std::make_shared<Err>(std::move(root_cause));
    return std::move(*this);
}

auto bld::Err::with_cause(Error_pt root_cause_ptr) && -> Err
{
    cause_ = std::move(root_cause_ptr);
    return std::move(*this);
}

auto std::formatter<bld::Err>::format(const bld::Err &err, std::format_context &ctx) const -> std::format_context::iterator
{
    auto out = ctx.out();
    switch (fmt) {
    case mode::plain:
        if (!err.msg.empty()) {
            out = std::format_to(out, "{}: {}", err.err.message(), err.msg);
        } else {
            out = std::format_to(out, "{}", err.err.message());
        }
        break;
    case mode::debug:
        if (!err.msg.empty()) {
            out = std::format_to(out, "[{}]: {}: {}", err.err.category().name(), err.err.message(), err.msg);
        } else {
            out = std::format_to(out, "[{}]: {}", err.err.category().name(), err.err.message());
        }
        break;
    }
    if (err.cause_) {
        out = std::format_to(out, "\n      -> caused by: {}", *err.cause_);
    }
    return out;
}

void bld::log::detail::Stream_proxy::operator=(std::ostream &os)
{
    ptr = &os;
}

std::ostream &bld::log::detail::Stream_proxy::get() const
{
    return *ptr;
}

auto bld::Logger::Default_logger_fn::operator()(std::ostream &stream, const Log_record &record) const -> void
{
    if (record.lvl < min_lvl) {
        return;
    }
    const auto s = style(record.lvl);
    if (use_color) {
        std::println(stream, "{}{}{}: {}", s.color, s.label, reset, record.str);
    } else {
        std::println(stream, "{}: {}", s.label, record.str);
    }
}

auto bld::Logger::set_logger_fn(Logger_fn_t fn, std::source_location loc) -> void
{
    std::lock_guard guard(config_mtx);
    if (logger_locked.load()) {
        throw std::runtime_error{
            std::format("{}:{}:{}: err: Logger already set, you cannot set it twice", loc.file_name(), loc.line(), loc.column())};
    }
    logger_locked = true;
    logger_fn = std::move(fn);
}

auto bld::Cmd::push(std::string_view s) -> void
{
    args_.emplace_back(s);
}

[[nodiscard]] auto bld::Cmd::argv() const -> std::vector<char *>
{
    auto out = args_ | std::views::transform([](std::string const &s) { return const_cast<char *>(s.c_str()); }) | std::ranges::to<std::vector>();
    out.push_back(nullptr);
    return out;
}

[[nodiscard]] auto bld::Cmd::str() const -> std::string
{
    return args_ | std::views::join_with(std::string_view{" "}) | std::ranges::to<std::string>();
}

auto bld::Cmd::reset() -> void
{
    args_.clear();
}

auto std::formatter<bld::Cmd>::format(const bld::Cmd &cmd, std::format_context &ctx) const -> std::format_context::iterator
{
    auto out = ctx.out();
    switch (fmt) {
    case mode::plain:
        out = std::format_to(out, "\"{}\"", cmd.str());
        break;
    case mode::unquoted:
        out = std::format_to(out, "{}", cmd.str());
        break;
    case mode::debug:
        out = std::format_to(out, "{}", cmd.args_);
        break;
    }
    return out;
}

bld::Cmd_loc::Cmd_loc(const Cmd &c, std::source_location l) : cmd(c), loc(l)
{}

bld::Proc::Proc(P_id id, const std::string &label_) : id_(id)
{
#ifdef _WIN32
    auto val = reinterpret_cast<std::uintptr_t>(id);
#else
    auto val = id;
#endif
    if (label_.empty()) {
        constexpr ::std::size_t size{12};
        char buf[size];
        if (auto [end, ec] = std::to_chars(buf, buf + size, val); ec == std::errc{}) {
            label = std::string{buf, end};
        }
    } else {
        label = label_;
    }
    status_ = Status{.state = State::running};
}

bld::Proc::~Proc()
{
#ifdef _WIN32
    if (id_ != nullptr && status_.state == State::running) {
        this->kill(9);
        std::ignore = this->wait();
    }
#else
    if (id_ > 0 && status_.state == State::running) {
        this->kill(SIGKILL);
        std::ignore = this->wait();
    }
#endif
}

#ifdef _WIN32
bld::Proc::Proc(Proc &&other) noexcept : id_(std::exchange(other.id_, nullptr)), status_(other.status_), label(std::move(other.label))
{}
#else
bld::Proc::Proc(Proc &&other) noexcept : id_(std::exchange(other.id_, -1)), status_(other.status_), label(std::move(other.label))
{}
#endif

auto bld::Proc::operator=(Proc &&other) noexcept -> Proc &
{
    if (this != &other) {
#ifdef _WIN32
        if (id_ != nullptr && status_.state == State::running) {
            kill(9);
            std::ignore = wait();
        }
        id_ = std::exchange(other.id_, nullptr);
#else
        if (id_ > 0 && status_.state == State::running) {
            kill(SIGKILL);
            std::ignore = wait();
        }
        id_ = std::exchange(other.id_, -1);
#endif
        status_ = other.status_;
        label = std::move(other.label);
    }
    return *this;
}

auto bld::Proc::wait() -> std::expected<Status, bld::Err>
{
#ifdef _WIN32
    if (!id_ || status_.state != State::running) {
        return status_;
    }

    if (::WaitForSingleObject(static_cast<HANDLE>(id_), INFINITE) == WAIT_FAILED) {
        return std::unexpected(bld::Err::erno(GetLastError(), "WaitForSingleObject failed"));
    }

    DWORD exit_code = 0;
    if (::GetExitCodeProcess(static_cast<HANDLE>(id_), &exit_code)) {
        status_ = {State::exited, static_cast<::std::uint8_t>(exit_code)};
    }
    ::CloseHandle(static_cast<HANDLE>(id_));
    id_ = nullptr;
    return status_;
#else
    if (id_ <= 0 || status_.state != State::running) {
        return status_;
    }

    int wstatus = 0;
    if (::waitpid(id_, &wstatus, 0) == -1) {
        if (errno == ECHILD) {
            status_ = {State::exited, 255};
            return status_;
        }
        return std::unexpected(bld::Err::erno(errno, "waitpid failed"));
    }

    update_status(wstatus);
    return status_;
#endif
}

auto bld::Proc::try_wait() -> std::expected<Status, bld::Err>
{
#ifdef _WIN32
    if (!id_ || status_.state != State::running) {
        return status_;
    }

    DWORD res = ::WaitForSingleObject(static_cast<HANDLE>(id_), 0);
    if (res == WAIT_OBJECT_0) {
        DWORD exit_code = 0;
        if (::GetExitCodeProcess(static_cast<HANDLE>(id_), &exit_code)) {
            status_ = {State::exited, static_cast<::std::uint8_t>(exit_code)};
        }
        ::CloseHandle(static_cast<HANDLE>(id_));
        id_ = nullptr;
    } else if (res == WAIT_FAILED) {
        return std::unexpected(bld::Err::erno(GetLastError(), "WaitForSingleObject failed"));
    }
    return status_;
#else
    if (id_ <= 0 || status_.state != State::running) {
        return status_;
    }

    int wstatus = 0;
    P_id res = ::waitpid(id_, &wstatus, WNOHANG);

    if (res == -1) {
        if (errno == ECHILD) {
            status_ = {State::exited, 255};
            return status_;
        }
        return std::unexpected(bld::Err::erno(errno, "waitpid WNOHANG failed"));
    }

    if (res > 0) {
        update_status(wstatus);
    }

    return status_;
#endif
}

auto bld::Proc::kill(int sig) -> void
{
#ifdef _WIN32
    if (id_ && status_.state == State::running) {
        ::TerminateProcess(static_cast<HANDLE>(id_), static_cast<UINT>(sig));
    }
#else
    if (id_ > 0 && status_.state == State::running) {
        ::kill(id_, sig);
    }
#endif
}

auto bld::Proc::pid() const -> P_id
{
    return id_;
}

auto bld::Proc::status() const -> Status
{
    return status_;
}

auto bld::Proc::is_running() const -> bool
{
    return status_.state == State::running;
}

auto bld::Proc::status_code() const -> int
{
    return +status_.code;
}

auto bld::Proc::spawn(const Cmd &cmd, const std::string &label_, const Io_routing &io) -> std::expected<Proc, bld::Err>
{
    if (cmd.empty()) {
        return std::unexpected(bld::Err::erc(std::errc::invalid_argument, "Command cannot be empty"));
    }
#ifdef _WIN32
    std::string cmd_str = cmd.str(); // CreateProcess requires a mutable string

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    // Convert file descriptors to Win32 HANDLEs
    si.hStdInput = (io.in == STDIN_FILENO) ? GetStdHandle(STD_INPUT_HANDLE) : reinterpret_cast<HANDLE>(_get_osfhandle(io.in));
    si.hStdOutput = (io.out == STDOUT_FILENO) ? GetStdHandle(STD_OUTPUT_HANDLE) : reinterpret_cast<HANDLE>(_get_osfhandle(io.out));

    if (io.merge_err_to_out) {
        si.hStdError = si.hStdOutput;
    } else {
        si.hStdError = (io.err == STDERR_FILENO) ? GetStdHandle(STD_ERROR_HANDLE) : reinterpret_cast<HANDLE>(_get_osfhandle(io.err));
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(
            nullptr,        // Application name
            cmd_str.data(), // Command line
            nullptr,        // Process attributes
            nullptr,        // Thread attributes
            TRUE,           // Inherit handles (Crucial for pipes!)
            0,              // Creation flags
            nullptr,        // Environment
            nullptr,        // Current directory
            &si,            // Startup info
            &pi))           // Process info
    {
        return std::unexpected(bld::Err::erno(GetLastError(), "CreateProcess failed").with_payload(cmd));
    }

    // We don't need the thread handle, just the process handle
    CloseHandle(pi.hThread);
    return Proc{pi.hProcess, label_.empty() ? cmd_str : label_};
#else
    P_id pid = ::fork();
    if (pid < 0) {
        return std::unexpected(bld::Err::erno(errno, "Fork failed").with_payload(cmd));
    }
    if (pid == 0) {
        if (io.in != STDIN_FILENO && io.in >= 0) {
            ::dup2(io.in, STDIN_FILENO);
        }
        if (io.out != STDOUT_FILENO && io.out >= 0) {
            ::dup2(io.out, STDOUT_FILENO);
        }
        if (io.merge_err_to_out) {
            ::dup2(STDOUT_FILENO, STDERR_FILENO);
        } else if (io.err != STDERR_FILENO && io.err >= 0) {
            ::dup2(io.err, STDERR_FILENO);
        }
        if (io.in > STDERR_FILENO) {
            ::close(io.in);
        }
        if (io.out > STDERR_FILENO) {
            ::close(io.out);
        }
        if (io.err > STDERR_FILENO) {
            ::close(io.err);
        }

        auto argv = cmd.argv();
        ::execvp(argv[0], argv.data());

        ::_exit(127);
    }

    return Proc{pid, label_.empty() ? cmd.str() : label_};
#endif
}

auto bld::Proc::wait_pid(P_id pid, int options) -> std::expected<Status, bld::Err>
{
#ifdef _WIN32
    (void)options;
    if (!pid) return Status{.state = State::exited, .code = 255};
    if (::WaitForSingleObject(static_cast<HANDLE>(pid), INFINITE) == WAIT_FAILED) {
        return std::unexpected(bld::Err::erno(GetLastError(), "wait_pid failed"));
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(static_cast<HANDLE>(pid), &exit_code);
    return Status{.state = State::exited, .code = static_cast<uint8_t>(exit_code)};
#else
    int wstatus = 0;
    P_id res = ::waitpid(pid, &wstatus, options);
    if (res == -1) {
        if (errno == ECHILD) return Status{.state = State::exited, .code = 255};
        return std::unexpected(bld::Err::erno(errno, "waitpid failed"));
    }
    if (res == 0) return Status{.state = State::running, .code = 0};
    return parse_status(wstatus);
#endif
}

auto bld::Proc::try_wait_pid(P_id pid) -> std::expected<Status, bld::Err>
{
#ifdef _WIN32
    if (!pid) return Status{.state = State::exited, .code = 255};
    DWORD res = ::WaitForSingleObject(static_cast<HANDLE>(pid), 0);
    if (res == WAIT_OBJECT_0) {
        DWORD exit_code = 0;
        GetExitCodeProcess(static_cast<HANDLE>(pid), &exit_code);
        return Status{.state = State::exited, .code = static_cast<uint8_t>(exit_code)};
    }
    return Status{.state = State::running, .code = 0};
#else
    return wait_pid(pid, WNOHANG);
#endif
}

auto bld::Proc::parse_status(int wstatus) -> Status
{
    Status s{};

#ifdef _WIN32
    // Windows process exit code
    if (wstatus == 0) {
        s.state = State::exited;
        s.code = 0;
    } else {
        s.state = State::exited;
        s.code = static_cast<std::uint8_t>(wstatus);
    }
#else
    if (WIFEXITED(wstatus)) {
        s.state = State::exited;
        s.code = static_cast<::std::uint8_t>(WEXITSTATUS(wstatus));
    } else if (WIFSIGNALED(wstatus)) {
        s.state = State::signaled;
        s.code = static_cast<::std::uint8_t>(WTERMSIG(wstatus));
    } else if (WIFSTOPPED(wstatus)) {
        s.state = State::stopped;
        s.code = static_cast<::std::uint8_t>(WSTOPSIG(wstatus));
    } else if (WIFCONTINUED(wstatus)) {
        s.state = State::continued;
        s.code = 0;
    }
#endif

    return s;
}

auto bld::Proc::update_status(int wstatus) -> void
{
    status_ = parse_status(wstatus);
}

auto std::formatter<bld::Proc::Status>::format(const bld::Proc::Status &s, std::format_context &ctx) const -> std::format_context::iterator
{
    auto out = ctx.out();
    switch (s.state) {
    case bld::Proc::State::running:
        out = std::format_to(out, "{{ running");
        break;
    case bld::Proc::State::exited:
        out = std::format_to(out, "{{ exited");
        break;
    case bld::Proc::State::signaled:
        out = std::format_to(out, "{{ signaled");
        break;
    case bld::Proc::State::stopped:
        out = std::format_to(out, "{{ stopped");
        break;
    case bld::Proc::State::continued:
        out = std::format_to(out, "{{ continued");
        break;
    default:
        std::unreachable();
    }
    switch (s.state) {
    case bld::Proc::State::exited:
        out = fmt == mode::debug ? std::format_to(out, ", code = {} }}", +s.code) : std::format_to(out, ", {} }}", +s.code);
        break;
    case bld::Proc::State::signaled:
        out = fmt == mode::debug ? std::format_to(out, ", sig = {} }}", +s.code) : std::format_to(out, ", {} }}", +s.code);
        break;
    case bld::Proc::State::running:
    case bld::Proc::State::stopped:
    case bld::Proc::State::continued:
        out = std::format_to(out, " }}");
        break;
    default:
        std::unreachable();
    }
    return out;
}

auto std::formatter<bld::Proc>::format(const bld::Proc &p, std::format_context &ctx) const -> std::format_context::iterator
{
    auto out = ctx.out();
    if (show_debug) {
        if (show_pid) {
            out = std::format_to(out, "{{ pid = {}, label = \"{}\", status = {:?} }}", p.id_, p.label, p.status_);
        } else {
            out = std::format_to(out, "{{ label = \"{}\", status = {:?} }}", p.label, p.status_);
        }
    } else {
        if (show_pid) {
            out = std::format_to(out, "{{ {}, \"{}\", {} }}", p.id_, p.label, p.status_);
        } else {
            out = std::format_to(out, "{{ \"{}\", {} }}", p.label, p.status_);
        }
    }
    return out;
}

bld::Owned_Fd::~Owned_Fd()
{
    close();
}

bld::Owned_Fd::Owned_Fd(Owned_Fd &&other) noexcept : handle_(std::exchange(other.handle_, Fd_view::INVALID))
{}

auto bld::Owned_Fd::operator=(Owned_Fd &&other) noexcept -> Owned_Fd &
{
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, Fd_view::INVALID);
    }
    return *this;
}

auto bld::Owned_Fd::close() -> void
{
    if (handle_ != Fd_view::INVALID && handle_ != Fd_view::DEFAULT_IN && handle_ != Fd_view::DEFAULT_OUT && handle_ != Fd_view::DEFAULT_ERR) {
#ifdef _WIN32
        ::_close(handle_);
#else
        ::close(handle_);
#endif
        handle_ = Fd_view::INVALID;
    }
}

bld::Owned_Fd::operator Fd_view() const
{
    return Fd_view{handle_};
}

auto bld::Owned_Fd::open(std::string_view path, Open_mode mode) -> std::expected<Owned_Fd, bld::Err>
{
    if (path.empty()) {
        return std::unexpected(bld::Err::erc(std::errc::invalid_argument, "Cannot open an empty path"));
    }
    int flags = 0;
#ifdef _WIN32
    switch (mode) {
    case Open_mode::read:
        flags = _O_RDONLY | _O_BINARY;
        break;
    case Open_mode::write:
        flags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY;
        break;
    case Open_mode::append:
        flags = _O_WRONLY | _O_CREAT | _O_APPEND | _O_BINARY;
        break;
    }
    int fd = ::_open(std::string{path}.c_str(), flags, 0666);
#else
    switch (mode) {
    case Open_mode::read:
        flags = O_RDONLY;
        break;
    case Open_mode::write:
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case Open_mode::append:
        flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    }
    int fd = ::open(std::string{path}.c_str(), flags, 0666);
#endif
    if (fd == Fd_view::INVALID) {
        return std::unexpected(bld::Err::erno(errno, std::format("Failed to open file: '{}'", path)));
    }
    return Owned_Fd{fd};
}

auto bld::async::operator()(Proc_config &cfg) const -> void
{
    cfg.async = true;
}

auto bld::label::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.label = val;
}

auto bld::label::operator()(bld::Capture_config &cfg) const -> void
{
    cfg.label = val;
}

auto bld::pipe::operator()(Proc_config &cfg) const -> void
{
    cfg.out = out;
    cfg.err = err;
    cfg.in = in;
    cfg.merge_err_and_out = false;
}

auto bld::out_s::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.out = fd;
}

auto bld::err_s::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.err = fd;
}

auto bld::in_s::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.in = fd;
}

auto bld::in_s::operator()(bld::Capture_config &cfg) const -> void
{
    cfg.in = fd;
}

bld::out_f::out_f(std::string_view path, bld::Open_mode mode)
{
    std::filesystem::path p{path};
    if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
        bld::log::f("Fatal: Parent directory for output file '{}' does not exist.", path);
        std::exit(EXIT_FAILURE);
    }
    auto res = bld::Owned_Fd::open(path, mode);
    if (!res) {
        bld::log::f("Fatal: Could not open output file '{}': {}", path, res.error().msg);
        std::exit(EXIT_FAILURE);
    }
    bld::log::i("Opened out fd: {} (file: '{}')", res->handle_, path);
    fd = std::move(*res);
}

auto bld::out_f::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.out = fd;
}

bld::err_f::err_f(std::string_view path, bld::Open_mode mode)
{
    std::filesystem::path p{path};

    if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
        bld::log::f("Parent directory for error log '{}' does not exist.", path);
        std::exit(EXIT_FAILURE);
    }
    auto res = bld::Owned_Fd::open(path, mode);
    if (!res) {
        bld::log::f("Could not open error file '{}': {}", path, res.error().msg);
        std::exit(EXIT_FAILURE);
    }
    bld::log::i("Opened err fd: {}: (file: '{}')", res->handle_, path);
    fd = std::move(*res);
}

auto bld::err_f::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.err = fd;
}

bld::in_f::in_f(std::string_view path)
{
    std::filesystem::path p{path};
    if (!std::filesystem::exists(p)) {
        bld::log::f("Fatal: Path '{}' for reading input doesn't exist.", path);
        std::exit(EXIT_FAILURE);
    }
    auto res = bld::Owned_Fd::open(path, bld::Open_mode::read);
    if (!res) {
        bld::log::f("Fatal: Could not open input file '{}': {}", path, res.error().msg);
        std::exit(EXIT_FAILURE);
    }
    bld::log::i("Opened in fd: {}: (file: '{}')", res->handle_, path);
    fd = std::move(*res);
}

auto bld::in_f::operator()(bld::Proc_config &cfg) const -> void
{
    cfg.in = fd;
}

auto bld::in_f::operator()(bld::Capture_config &cfg) const -> void
{
    cfg.in = fd;
}

bld::cap_out::cap_out(std::string &s) : ptr(&s)
{}

auto bld::cap_out::operator()(Capture_config &cfg) const -> void
{
    cfg.out = ptr;
}

bld::cap_err::cap_err(std::string &s) : ptr(&s)
{}

auto bld::cap_err::operator()(Capture_config &cfg) const -> void
{
    cfg.err = ptr;
}

bld::cap_merge::cap_merge(std::string &s) : ptr(&s)
{}

auto bld::cap_merge::operator()(Capture_config &cfg) const -> void
{
    cfg.out = ptr;
    cfg.merge_out_err = true;
}

bld::in_str::in_str(std::string_view s) : val(s)
{}

auto bld::in_str::operator()(Capture_config &cfg) const -> void
{
    cfg.in_str = val;
}

auto bld::details::execute(const bld::Cmd &cmd, const Proc_config &cfg, std::source_location loc) -> std::expected<bld::Proc, bld::Err>
{
    Proc::Io_routing io{};

    if (cfg.in.is_valid()) {
        bld::log::i("Routing input from fd: {} for cmd: {:?}", cfg.in.val, cmd);
        io.in = cfg.in.val;
    }
    if (cfg.out.is_valid()) {
        bld::log::i("Routing output to fd: {} for cmd: {:?}", cfg.out.val, cmd);
        io.out = cfg.out.val;
    }
    if (cfg.err.is_valid()) {
        bld::log::i("Routing error to fd: {} for cmd: {:?}", cfg.err.val, cmd);
        io.err = cfg.err.val;
    }
    io.merge_err_to_out = cfg.merge_err_and_out;
    bld::log::i("Executing command: {:?}", cmd);

    return bld::Proc::spawn(cmd, cfg.label, io)
        .transform_error([&loc](bld::Err err) {
            bld::log::e("at: {}:{}: {}", loc.file_name(), loc.line(), err);
            return err;
        })
        .and_then([&cfg, &loc](bld::Proc proc) -> std::expected<bld::Proc, bld::Err> {
            if (!cfg.async) {
                auto status = proc.wait();
                if (!status) {
                    bld::log::e("at: {}:{}: Wait failed: {}", loc.file_name(), loc.line(), status.error());
                    return std::unexpected(status.error());
                }
            }
            return proc;
        });
}

auto bld::details::capture_execute(const bld::Cmd &cmd, bld::Capture_config &cap_cfg, std::source_location loc) -> std::expected<bld::Proc::Status, bld::Err>
{
    bld::log::i("Setting up capture for cmd: {:?}", cmd);

    auto make_pipe = [](int p[2], const char *name) -> std::expected<void, bld::Err> {
#ifdef _WIN32
        if (::_pipe(p, 4096, _O_BINARY) == -1) {
            return std::unexpected(bld::Err::erno(errno, std::format("{} pipe failed", name)));
        }
#else
        if (::pipe(p) == -1) {
            return std::unexpected(bld::Err::erno(errno, std::format("{} pipe failed", name)));
        }
        ::fcntl(p[0], F_SETFD, FD_CLOEXEC);
        ::fcntl(p[1], F_SETFD, FD_CLOEXEC);
#endif
        return {};
    };

    auto close_fd = [](int fd) {
        if (fd >= 0) {
#ifdef _WIN32
            ::_close(fd);
#else
            ::close(fd);
#endif
        }
    };

    auto read_fd = [](int fd, void* buf, unsigned int count) -> int {
#ifdef _WIN32
        return ::_read(fd, buf, count);
#else
        return static_cast<int>(::read(fd, buf, count));
#endif
    };

    auto write_fd = [](int fd, const void* buf, unsigned int count) -> int {
#ifdef _WIN32
        return ::_write(fd, buf, count);
#else
        return static_cast<int>(::write(fd, buf, count));
#endif
    };

    int pipe_out[2]{-1, -1}, pipe_err[2]{-1, -1}, pipe_in[2]{-1, -1};
    Proc_config run_cfg{.label = cap_cfg.label, .async = true, .loc = cap_cfg.loc, .in = cap_cfg.in};

    if (!cap_cfg.in_str.empty()) {
        if (auto res = make_pipe(pipe_in, "stdin"); !res) return std::unexpected(res.error());
        run_cfg.in = Fd_view{pipe_in[0]};
        bld::log::i("Created stdin pipe (read: {}, write: {})", pipe_in[0], pipe_in[1]);
    }

    if (cap_cfg.out) {
        if (auto res = make_pipe(pipe_out, "stdout"); !res) return std::unexpected(res.error());
        run_cfg.out = Fd_view{pipe_out[1]};
        bld::log::i("Created stdout pipe (read: {}, write: {})", pipe_out[0], pipe_out[1]);
    }

    if (cap_cfg.merge_out_err) {
        run_cfg.merge_err_and_out = true;
        bld::log::i("Merging stderr into stdout pipe");
    } else if (cap_cfg.err) {
        if (auto res = make_pipe(pipe_err, "stderr"); !res) return std::unexpected(res.error());
        run_cfg.err = Fd_view{pipe_err[1]};
        bld::log::i("Created stderr pipe (read: {}, write: {})", pipe_err[0], pipe_err[1]);
    }

    auto proc_res = bld::details::execute(cmd, run_cfg, loc);

    // The parent must immediately close the child's ends of the pipes, 
    // otherwise the read loops below will block forever waiting for EOF.
    if (cap_cfg.out) close_fd(pipe_out[1]);
    if (cap_cfg.err && !cap_cfg.merge_out_err) close_fd(pipe_err[1]);
    if (!cap_cfg.in_str.empty()) close_fd(pipe_in[0]);

    if (!proc_res) {
        // Clean up our remaining ends if spawn completely failed
        if (cap_cfg.out) close_fd(pipe_out[0]);
        if (cap_cfg.err && !cap_cfg.merge_out_err) close_fd(pipe_err[0]);
        if (!cap_cfg.in_str.empty()) close_fd(pipe_in[1]);
        return std::unexpected(proc_res.error());
    }

    auto &proc = *proc_res;
    std::vector<std::thread> io_threads;

    if (cap_cfg.out) {
        io_threads.emplace_back([fd = pipe_out[0], ptr = cap_cfg.out, read_fd, close_fd]() {
            char buf[4096];
            while (true) {
                int bytes = read_fd(fd, buf, sizeof(buf));
                if (bytes > 0) ptr->append(buf, static_cast<std::size_t>(bytes));
                else break;
            }
            close_fd(fd);
        });
    }

    if (cap_cfg.err && !cap_cfg.merge_out_err) {
        io_threads.emplace_back([fd = pipe_err[0], ptr = cap_cfg.err, read_fd, close_fd]() {
            char buf[4096];
            while (true) {
                int bytes = read_fd(fd, buf, sizeof(buf));
                if (bytes > 0) ptr->append(buf, static_cast<std::size_t>(bytes));
                else break;
            }
            close_fd(fd);
        });
    }

    if (!cap_cfg.in_str.empty()) {
        io_threads.emplace_back([fd = pipe_in[1], str = cap_cfg.in_str, write_fd, close_fd]() {
            std::size_t written = 0;
            while (written < str.size()) {
                int bytes = write_fd(fd, str.data() + written, static_cast<unsigned int>(str.size() - written));
                if (bytes > 0) {
                    written += static_cast<std::size_t>(bytes);
                } else if (bytes == -1 && errno != EINTR && errno != EAGAIN) {
                    break;
                }
            }
            close_fd(fd);
        });
    }

    // Join threads to ensure all I/O is perfectly drained before waiting on PID
    for (auto &t : io_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return proc.wait();
}

auto bld::is_outdated(std::string_view target, std::string_view source) -> bool
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(target, ec)) {
        bld::log::w("Target '{}' does not exist. Rebuild required.", target);
        return true;
    }

    auto target_time = fs::last_write_time(target, ec);
    if (ec) {
        bld::log::w("Failed to read timestamp for target '{}': {}. Defaulting to rebuild.", target, ec.message());
        return true;
    }

    if (!fs::exists(source, ec)) {
        bld::log::e("Source file missing: '{}'. Forcing rebuild.", source);
        return true;
    }

    auto source_time = fs::last_write_time(source, ec);
    if (ec) {
        bld::log::w("Failed to read timestamp for source '{}': {}. Defaulting to rebuild.", source, ec.message());
        return true;
    }

    bool outdated = target_time < source_time;

    if (outdated) {
        bld::log::i("Target '{}' is outdated relative to '{}'.", target, source);
    } else {
        bld::log::d("Target '{}' is up to date.", target);
    }

    return outdated;
}

auto bld::get_current_cxx_compiler() -> std::string_view
{
    using namespace std::literals::string_view_literals;

#ifdef __clang__
    auto compiler = "clang++"sv;
#elif defined(__GNUC__)
    auto compiler = "g++"sv;
#elif defined(_MSC_VER)
    auto compiler = "cl"sv;
#else
    auto compiler = "c++"sv;
#endif
    return compiler;
}

auto bld::rebuild_this_when_needed(int argc, char **argv, std::string_view compiler, std::source_location loc) -> void
{
    if (argv == nullptr || argc <= 0) {
        return;
    }

    std::span<char *> args_span{argv, static_cast<std::size_t>(argc)};
    namespace fs = std::filesystem;
    std::string target = args_span[0];

    std::string source = loc.file_name();

    if (!bld::is_outdated(target, source)) {
        return;
    }

    bld::log::i("Rebuilding...", target, source);

    std::string old_target = target + ".old";
    std::error_code ec;

    fs::rename(target, old_target, ec);
    if (ec) {
        bld::log::w("Failed to rename currently running executable: {}", ec.message());
    }

    const char *cxx = nullptr;

    if (!compiler.empty()) {
        cxx = compiler.data();
    } else {
        cxx = bld::get_current_cxx_compiler().data();
    }

    bld::Cmd build_cmd{cxx, "-o", target, source, "-std=c++23", "-O3", "-Wall", "-Wextra"};
    #ifdef _WIN32
    #ifdef __GNUC__
        build_cmd.push("-lstdc++exp");
    #endif
    #endif

    auto proc = bld::run(build_cmd);
    if (!proc || proc->status_.code != 0) {
        bld::log::e("FATAL: Failed to rebuild build script.");
        fs::rename(old_target, target, ec);
        std::exit(1);
    }

    bld::log::i("Successfully rebuilt! Restarting...");

    std::vector<char *> c_argv(args_span.begin(), args_span.end());
    c_argv.push_back(nullptr);

    ::execvp(target.c_str(), c_argv.data());

    bld::log::f("FATAL: Failed to restart build script after compilation: {}", std::strerror(errno));
    std::exit(1);
}

auto bld::rebuild_this_when_needed_ext(int argc, char **argv, std::vector<std::string> flags, std::string_view compiler, std::source_location loc)
    -> void
{
    if (argv == nullptr || argc <= 0) {
        return;
    }

    std::span<char *> args_span{argv, static_cast<std::size_t>(argc)};
    namespace fs = std::filesystem;
    std::string target = args_span[0];

    std::string source = loc.file_name();

    if (!bld::is_outdated(target, std::array<std::string_view, 2>{source, std::string_view(__FILE__)})) {
        return;
    }

    bld::log::i("Rebuilding...", target, source);

    std::string old_target = target + ".old";
    std::error_code ec;

    fs::rename(target, old_target, ec);
    if (ec) {
        bld::log::w("Failed to rename currently running executable: {}", ec.message());
    }

    const char *cxx = nullptr;

    if (!compiler.empty()) {
        cxx = compiler.data();
    } else {
        cxx = bld::get_current_cxx_compiler().data();
    }

    bld::Cmd build_cmd{cxx, "-o", target, source, "-std=c++23", "-O3", "-Wall", "-Wextra"};

#ifdef _WIN32
#ifdef __GNUC__
    build_cmd.push("-lstdc++exp");
#endif
#endif

    for (const auto &f : flags) {
        build_cmd.push(f);
    }

    auto proc = bld::run(build_cmd);
    if (!proc || proc->status_.code != 0) {
        bld::log::e("FATAL: Failed to rebuild build script.");
        fs::rename(old_target, target, ec);
        std::exit(1);
    }

    bld::log::i("Successfully rebuilt! Restarting...");

    std::vector<char *> c_argv(args_span.begin(), args_span.end());
    c_argv.push_back(nullptr);

    ::execvp(target.c_str(), c_argv.data());

    bld::log::f("FATAL: Failed to restart build script after compilation: {}", std::strerror(errno));
    std::exit(1);
}

auto bld::wait_all(std::span<bld::Proc> procs) -> std::expected<std::size_t, bld::Err>
{
#ifdef _WIN32
    std::vector<HANDLE> handles;
    std::vector<bld::Proc *> proc_ptrs; // Keep a parallel array to know which Proc finished

    for (auto &proc : procs) {
        if (proc.is_running() && proc.pid() != nullptr) {
            handles.push_back(static_cast<HANDLE>(proc.pid()));
            proc_ptrs.push_back(&proc);
        }
    }

    const std::size_t total = handles.size();
    if (total == 0) {
        return 0;
    }

    bld::log::i("Waiting for {} processes, asynchronously", total);
    std::size_t completed = 0;
    bool has_errors = false;

    // Windows has a MAXIMUM_WAIT_OBJECTS limit of 64.
    // Since we batch by hardware_concurrency, this is perfectly safe.
    while (!handles.empty()) {
        // Wait for ANY process to finish (bWaitAll = FALSE)
        DWORD wait_res = ::WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);

        if (wait_res >= WAIT_OBJECT_0 && wait_res < WAIT_OBJECT_0 + handles.size()) {
            DWORD idx = wait_res - WAIT_OBJECT_0;
            bld::Proc *p = proc_ptrs[idx];
            HANDLE h = handles[idx];

            DWORD exit_code = 0;
            ::GetExitCodeProcess(h, &exit_code);

            p->status_ = bld::Proc::Status{bld::Proc::State::exited, static_cast<uint8_t>(exit_code)};
            ::CloseHandle(h);
            p->id_ = nullptr; // Clear the PID/Handle

            completed++;
            int percentage = static_cast<int>((completed * 100) / total);

            if (exit_code != 0) {
                bld::log::e("[{:>3}%] Process '{}' failed: exited with code {}", percentage, p->label, exit_code);
                has_errors = true;
            } else {
                bld::log::i("[{:>3}%] Process '{}': completed.", percentage, p->label);
            }

            // Remove the finished handle by swapping with the last element and popping
            handles[idx] = handles.back();
            handles.pop_back();
            proc_ptrs[idx] = proc_ptrs.back();
            proc_ptrs.pop_back();

        } else if (wait_res == WAIT_FAILED) {
            bld::log::e("Error in waiting for procs.");
            return std::unexpected(bld::Err::erno(GetLastError(), "WaitForMultipleObjects failed").with_payload(completed));
        } else {
            bld::log::e("Unexpected wait result.");
            return std::unexpected(
                bld::Err::erc(std::errc::operation_canceled, "WaitForMultipleObjects returned unexpected status").with_payload(completed));
        }
    }

    if (has_errors) {
        return std::unexpected(bld::Err::erc(std::errc::operation_canceled, "One or more async processes failed").with_payload(completed));
    }

    return completed;

#else
    std::size_t remaining{0};
    std::size_t completed{0};
    bool has_errors = false;

    for (const auto &proc : procs) {
        if (proc.is_running()) {
            remaining++;
        }
    }
    const std::size_t total = remaining;
    if (total == 0) {
        return {};
    }

    bld::log::i("Waiting for {} processes, asynchronously", total);
    while (remaining > 0) {
        int wstatus = 0;
        pid_t pid = ::waitpid(-1, &wstatus, 0);

        if (pid > 0) {
            auto status = bld::Proc::parse_status(wstatus);

            for (auto &proc : procs) {
                if (proc.pid() == pid && proc.is_running()) {
                    proc.status_ = status;
                    remaining--;

                    completed = total - remaining;
                    int percentage = static_cast<int>((completed * 100) / total);

                    if (status.code != 0) {
                        bld::log::e("[{:>3}%] Process '{}' failed (pid: {}): exited with code {}", percentage, proc.label, pid, status.code);
                        has_errors = true;
                    } else {
                        bld::log::i("[{:>3}%] Process '{}' (pid: {}): completed.", percentage, proc.label, pid);
                    }

                    break;
                }
            }
        } else if (pid == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD) {
                break;
            }
            bld::log::e("Error in waiting for procs.");
            return std::unexpected(bld::Err::erno(errno, "waitpid failed in wait_all").with_payload(completed));
        }
    }

    if (has_errors) {
        return std::unexpected(bld::Err::erc(std::errc::operation_canceled, "One or more async processes failed").with_payload(completed));
    }

    return completed;
#endif
}

auto bld::run(std::span<bld::Task> tasks, std::size_t max_jobs) -> std::expected<void, bld::Err>
{
    if (max_jobs == 0) {
        max_jobs = std::thread::hardware_concurrency();
        if (max_jobs == 0) {
            max_jobs = 1;
        }
    }

    bld::log::i("Building in batches of: {}", max_jobs);

    std::vector<bld::Proc> active_procs;
    active_procs.reserve(max_jobs);
    std::size_t batch_n{1};
    std::size_t completed{0};

    bld::log::i("Scheduling batch number: {}", batch_n);
    for (const auto &t : tasks) {
        if (active_procs.size() >= max_jobs) {
            auto wait_res = bld::wait_all(active_procs);
            if (!wait_res) {
                completed += std::any_cast<std::size_t>(wait_res.error().payload);
                bld::log::i("Waiting failed; total completed tasks: {}", completed);

                auto err = wait_res.error();
                err.payload = completed;
                return std::unexpected(std::move(err));
            }
            active_procs.clear();
            completed += *wait_res;
            bld::log::i("Scheduling batch number: {}", ++batch_n);
            bld::log::i("Remaining tasks: {}", (tasks.size() - completed));
        }

        auto proc_res = bld::details::execute(t.cmd, t.cfg, t.cfg.loc);
        if (!proc_res) {
            return std::unexpected(proc_res.error());
        }
        active_procs.push_back(std::move(*proc_res));
    }

    if (!active_procs.empty()) {
        auto wait_res = bld::wait_all(active_procs);
        if (!wait_res) {
            completed += std::any_cast<std::size_t>(wait_res.error().payload);
            auto err = wait_res.error();
            err.payload = completed;
            return std::unexpected(std::move(err));
        }
    }

    bld::log::i("Done; total completed tasks: {}", completed);
    return {};
}

auto bld::Config::get() -> Config &
{
    static Config instance;
    return instance;
}

auto bld::Config::operator[](std::string_view key) const -> Proxy
{
    return Proxy{this, key};
}

auto bld::Config::add_option(std::string_view flag, val_t type, std::string_view desc, value_type def, std::vector<std::string> valid_choices)
    -> Config &
{
    options[flag] = Option{type, std::string(desc), std::move(def), std::move(valid_choices)};
    return *this;
}

auto bld::Config::print_help(std::string_view prog_name, std::string_view specific_opt) const -> void
{
    std::string help_text;

    if (!specific_opt.empty() && options.contains(specific_opt)) {
        const auto &opt = options.at(specific_opt);
        std::string_view type_str;
        switch (opt.type) {
        case Bool:
            type_str = "bool";
            break;
        case Int:
            type_str = "int";
            break;
        case Double:
            type_str = "double";
            break;
        case String:
            type_str = "string";
            break;
        case String_arr:
            type_str = "string[]";
            break;
        }
        std::format_to(std::back_inserter(help_text), "Option: {}\n", specific_opt);
        std::format_to(std::back_inserter(help_text), "  Type: {}\n", type_str);
        std::format_to(std::back_inserter(help_text), "  Desc: {}", opt.description);
        if (!opt.choices.empty()) {
            auto joined = opt.choices | std::views::join_with(std::string_view{"|"}) | std::ranges::to<std::string>();
            std::format_to(std::back_inserter(help_text), "\n  Choices: [{}]", joined);
        }
        bld::log::i("{}", help_text);
        return;
    }

    if (!specific_opt.empty()) {
        bld::log::w("Option '{}' is not registered. Showing general help.", specific_opt);
    }

    std::format_to(std::back_inserter(help_text), "Usage: {} [options]\nOptions:", prog_name);

    for (const auto &[flag, opt] : options) {
        std::string_view type_str;
        switch (opt.type) {
        case Bool:
            type_str = "bool";
            break;
        case Int:
            type_str = "int";
            break;
        case Double:
            type_str = "double";
            break;
        case String:
            type_str = "string";
            break;
        case String_arr:
            type_str = "string[]";
            break;
        }

        std::string def_str = "null";
        if (auto *p = std::get_if<int>(&opt.default_val); p) {
            def_str = std::format("{}", *p);
        } else if (auto *p = std::get_if<bool>(&opt.default_val); p) {
            def_str = *p ? "true" : "false";
        } else if (auto *p = std::get_if<double>(&opt.default_val); p) {
            def_str = std::format("{}", *p);
        } else if (auto *p = std::get_if<std::string>(&opt.default_val); p) {
            def_str = std::format("\"{}\"", *p);
        }

        std::format_to(std::back_inserter(help_text), "\n  {:<15} [{:<8}] : {}", flag, type_str, opt.description);

        if (!opt.choices.empty()) {
            auto joined = opt.choices | std::views::join_with(std::string_view{"|"}) | std::ranges::to<std::string>();
            std::format_to(std::back_inserter(help_text), " [{}]", joined);
        }

        std::format_to(std::back_inserter(help_text), " (default: {})", def_str);
    }

    bld::log::i("{}", help_text);
}

auto bld::Config::parse(int argc, char *argv[]) -> void
{
    std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    std::string_view prog_name = args.empty() ? "bld" : args[0];

    for (auto i{1uz}; i < static_cast<std::size_t>(argc); ++i) {
        std::string_view curr = args[i];
        if (curr == "-h" || curr == "--help") {
            std::string_view specific = "";
            if (i > 1 && args[i - 1][0] != '-') {
                specific = args[i - 1];
            } else if (i + 1 < args.size() && args[i + 1][0] != '-') {
                specific = args[i + 1];
            }
            print_help(prog_name, specific);
            std::exit(0);
        }
    }

    for (const auto &[flag, opt] : options) {
        data[flag] = opt.default_val;
    }

    for (auto i{1uz}; i < static_cast<std::size_t>(argc); ++i) {
        std::string_view curr = args[i];
        auto eq_idx = curr.find_first_of('=');

        if (eq_idx == std::string_view::npos) {
            data[curr] = true;
            continue;
        }

        std::string_view key = curr.substr(0, eq_idx);
        std::string_view val = curr.substr(eq_idx + 1);

        if (options.contains(key)) {
            val_t expected_type = options[key].type;

            if (expected_type == Bool) {
                data[key] = (val == "true" || val == "1");
            } else if (expected_type == Int) {
                int v{};
                auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), v);
                if (ec == std::errc{} && p == val.data() + val.size()) {
                    data[key] = v;
                } else {
                    bld::log::e("FATAL: Option '{}' expects an integer, got '{}'", key, val);
                    std::exit(1);
                }
            } else if (expected_type == Double) {
                double v{};
                auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), v);
                if (ec == std::errc{} && p == val.data() + val.size()) {
                    data[key] = v;
                } else {
                    bld::log::e("FATAL: Option '{}' expects a double, got '{}'", key, val);
                    std::exit(1);
                }
            } else if (expected_type == String) {
                std::string string_val(val);
                if (!options[key].choices.empty()) {
                    auto &ch = options[key].choices;
                    if (std::find(ch.begin(), ch.end(), string_val) == ch.end()) {
                        bld::log::e("FATAL: Invalid choice '{}' for option '{}'.", string_val, key);
                        std::exit(1);
                    }
                }
                data[key] = std::move(string_val);
            } else if (expected_type == String_arr) {
                if (!std::holds_alternative<std::vector<std::string>>(data[key])) {
                    data[key] = std::vector<std::string>{};
                }
                std::get<std::vector<std::string>>(data[key]).push_back(std::string(val));
            }
            continue;
        }

        int value_int{};
        auto [ptr_i, ec_i] = std::from_chars(val.data(), val.data() + val.size(), value_int);
        if (ec_i == std::errc{} && ptr_i == val.data() + val.size()) {
            data[key] = value_int;
            continue;
        }

        double value_double{};
        auto [ptr_d, ec_d] = std::from_chars(val.data(), val.data() + val.size(), value_double);
        if (ec_d == std::errc{} && ptr_d == val.data() + val.size()) {
            data[key] = value_double;
            continue;
        }

        data[key] = std::string(val);
    }
}

bld::Config::Proxy::operator bool() const
{
    auto it = cfg->data.find(key);
    if (it == cfg->data.end()) {
        return false;
    }
    if (auto *b = std::get_if<bool>(&it->second)) {
        return *b;
    }
    return true;
}

bld::Config::Proxy::operator std::string() const
{
    auto it = cfg->data.find(key);
    if (it == cfg->data.end()) {
        throw std::runtime_error(std::format("Config error: '{}' not found", key));
    }
    if (auto *p = std::get_if<std::string>(&it->second)) {
        return *p;
    }
    if (auto *p = std::get_if<int>(&it->second)) {
        return std::to_string(*p);
    }
    if (auto *p = std::get_if<double>(&it->second)) {
        return std::to_string(*p);
    }
    if (auto *p = std::get_if<bool>(&it->second)) {
        return *p ? "true" : "false";
    }
    throw std::runtime_error(std::format("Config error: Type mismatch for '{}'", key));
}

bld::Config::Proxy::operator int() const
{
    if (auto res = cfg->get_val<int>(key)) {
        return *res;
    } else {
        throw std::runtime_error(std::format("Config error: {}", res.error().msg));
    }
}

bld::Config::Proxy::operator double() const
{
    if (auto res = cfg->get_val<double>(key)) {
        return *res;
    } else {
        throw std::runtime_error(std::format("Config error: {}", res.error().msg));
    }
}

bld::Config::Proxy::operator std::vector<std::string>() const
{
    if (auto res = cfg->get_val<std::vector<std::string>>(key)) {
        return *res;
    } else {
        throw std::runtime_error(std::format("Config error: {}", res.error().msg));
    }
}

auto std::formatter<std::unordered_map<std::string_view, bld::Config::value_type>>::format(
    const std::unordered_map<std::string_view, bld::Config::value_type> &m, std::format_context &ctx) const -> std::format_context::iterator
{
    auto out = ctx.out();
    for (const auto &[k, v] : m) {
        std::format_to(out, "{}: ", k);
        if (auto *p = std::get_if<int>(&v); p) {
            std::format_to(out, "(i){}", *p);
        } else if (auto *p = std::get_if<bool>(&v); p) {
            std::format_to(out, "(b){}", *p);
        } else if (auto *p = std::get_if<double>(&v); p) {
            std::format_to(out, "(d){}", *p);
        } else if (auto *p = std::get_if<std::string>(&v); p) {
            std::format_to(out, "(s){}", *p);
        } else if (auto *p = std::get_if<std::vector<std::string>>(&v); p) {
            std::format_to(out, "(s[]){}", *p);
        } else {
            std::format_to(out, "unknown");
        }
    }
    return out;
}

bld::test::Frontier::Frontier(std::ptrdiff_t max_d) : data(2 * max_d + 1, 0), offset(max_d)
{}

bld::test::compute_diff_op::operator bool() const
{
    return same;
}

auto bld::test::Frontier::operator[](std::ptrdiff_t k) -> std::ptrdiff_t &
{
    return data[k + offset];
}

auto bld::test::Frontier::operator[](std::ptrdiff_t k) const -> std::ptrdiff_t
{
    return data[k + offset];
}

auto bld::test::compute_diff(std::span<const std::string_view> original, std::span<const std::string_view> updated) -> compute_diff_op
{
    const std::ptrdiff_t n = original.size();
    const std::ptrdiff_t m = updated.size();
    const std::ptrdiff_t max_edits = n + m;

    if (max_edits == 0) {
        return compute_diff_op{.same = true, .edits = {}};
    }

    std::vector<Frontier> history;
    history.reserve(max_edits);

    Frontier current_frontier{max_edits};
    bool reached_end = false;
    std::ptrdiff_t end_x = 0;
    std::ptrdiff_t end_y = 0;

    for (std::ptrdiff_t d = 0; d <= max_edits; ++d) {
        history.push_back(current_frontier);

        for (std::ptrdiff_t k = -d; k <= d; k += 2) {
            std::ptrdiff_t x = 0;

            bool moving_down = (k == -d) || (k != d && current_frontier[k - 1] <= current_frontier[k + 1]);

            if (moving_down) {
                x = current_frontier[k + 1];
            } else {
                x = current_frontier[k - 1] + 1;
            }

            std::ptrdiff_t y = x - k;

            while (x < n && y < m && original[x] == updated[y]) {
                x++;
                y++;
            }

            current_frontier[k] = x;

            if (x >= n && y >= m) {
                reached_end = true;
                end_x = x;
                end_y = y;
                break;
            }
        }
        if (reached_end) {
            break;
        }
    }

    std::vector<Edit> script;
    std::ptrdiff_t x = end_x;
    std::ptrdiff_t y = end_y;

    for (std::ptrdiff_t d = history.size() - 1; d > 0; --d) {
        const Frontier &past = history[d];
        std::ptrdiff_t k = x - y;

        bool moving_down = (k == -d) || (k != d && past[k - 1] <= past[k + 1]);
        std::ptrdiff_t prev_k = moving_down ? k + 1 : k - 1;

        std::ptrdiff_t prev_x = past[prev_k];
        if (!moving_down) {
            prev_x++;
        }
        std::ptrdiff_t prev_y = prev_x - k;

        while (x > prev_x && y > prev_y) {
            script.push_back({Edit_type::keep, original[x - 1]});
            x--;
            y--;
        }

        if (moving_down) {
            script.push_back({Edit_type::insert, updated[y - 1]});
            y--;
        } else {
            script.push_back({Edit_type::remove, original[x - 1]});
            x--;
        }
    }

    while (x > 0 && y > 0) {
        script.push_back({Edit_type::keep, original[x - 1]});
        x--;
        y--;
    }

    std::ranges::reverse(script);

    bool is_same = (script.size() == static_cast<std::size_t>(n)) && (n == m);

    return compute_diff_op{.same = is_same, .edits = script};
}

auto bld::test::split_lines(std::string_view text) -> std::vector<std::string_view>
{
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

auto bld::test::compute_diff(std::initializer_list<std::string_view> original, std::initializer_list<std::string_view> updated) -> compute_diff_op
{
    return compute_diff(std::vector<std::string_view>{original}, std::vector<std::string_view>{updated});
}

auto bld::test::compute_diff(std::string_view original, std::string_view updated) -> compute_diff_op
{
    return compute_diff(split_lines(original), split_lines(updated));
}

auto std::formatter<bld::test::compute_diff_op>::format(const bld::test::compute_diff_op &diff, std::format_context &ctx) const
    -> std::format_context::iterator
{
    auto out = ctx.out();

    if (diff.same) {
        return std::format_to(out, "Files match.\n");
    }

    for (const auto &e : diff.edits) {
        switch (e.type) {
        case bld::test::Edit_type::keep:
            out = std::format_to(out, "  {}\n", e.content);
            break;
        case bld::test::Edit_type::insert:
            if (use_color) {
                out = std::format_to(out, "\033[32m+ {}\033[0m\n", e.content);
            } else {
                out = std::format_to(out, "+ {}\n", e.content);
            }
            break;
        case bld::test::Edit_type::remove:
            if (use_color) {
                out = std::format_to(out, "\033[31m- {}\033[0m\n", e.content);
            } else {
                out = std::format_to(out, "- {}\n", e.content);
            }
            break;
        }
    }
    return out;
}

auto bld::fs::make_dir_if_not_exists(std::string_view path, bool create_parents, std::source_location loc) noexcept -> bool
{
    namespace fs = std::filesystem;

    if (path.empty()) {
        bld::log::w("({}:{}) No directory was created because of empty path.", loc.file_name(), loc.line());
        return false;
    }

    std::error_code ec;
    const bool created = create_parents ? fs::create_directories(fs::path{path}, ec) : fs::create_directory(fs::path{path}, ec);

    if (ec) {
        bld::log::w("Error while creating the dir: {}", ec.message());
        return false;
    }

    if (created) {
        bld::log::i("Created new dir: {}", path);
    }

    return created;
}

namespace bld::fs {

bool Dir_entry::is_file() const noexcept
{
    return type == std::filesystem::file_type::regular;
}

bool Dir_entry::is_dir() const noexcept
{
    return type == std::filesystem::file_type::directory;
}

bool Dir_entry::is_symlink() const noexcept
{
    return type == std::filesystem::file_type::symlink;
}

bool Dir_entry::is_hidden() const noexcept
{
    const auto n = filename();
    return !n.empty() && n.front() == '.';
}

std::string_view Dir_entry::extension() const noexcept
{
    return path.extension().string();
}

std::string_view Dir_entry::stem() const noexcept
{
    return path.stem().string();
}

std::string_view Dir_entry::filename() const noexcept
{
    return path.filename().string();
}

std::string_view Dir_entry::parent() const noexcept
{
    return path.parent_path().string();
}

bool Walk_error::is_fs_error() const noexcept
{
    return kind == Kind::fs;
}

bool Walk_error::is_visitor_error() const noexcept
{
    return kind == Kind::visitor;
}

std::string Walk_error::message() const
{
    return code.message();
}

Dir_walker::Dir_walker(std::string_view root) : root_{root}
{}

Dir_walker::Dir_walker(std::filesystem::path root) : root_{std::move(root)}
{}

Dir_walker::Dir_walker(const std::string &root) : root_{root}
{}

auto Dir_walker::recursive(bool v) noexcept -> Dir_walker &
{
    recursive_ = v;
    return *this;
}

auto Dir_walker::flat() noexcept -> Dir_walker &
{
    recursive_ = false;
    return *this;
}

auto Dir_walker::include_dirs(bool v) noexcept -> Dir_walker &
{
    include_dirs_ = v;
    return *this;
}

auto Dir_walker::include_hidden(bool v) noexcept -> Dir_walker &
{
    include_hidden_ = v;
    return *this;
}

auto Dir_walker::follow_symlinks(bool v) noexcept -> Dir_walker &
{
    follow_symlinks_ = v;
    return *this;
}

auto Dir_walker::max_depth(int v) noexcept -> Dir_walker &
{
    max_depth_ = v;
    return *this;
}

auto Dir_walker::ext(std::string e) -> Dir_walker &
{
    if (!e.empty() && e.front() != '.') {
        e = '.' + e;
    }
    return where([e = std::move(e)](const Dir_entry &entry) { return entry.extension() == e; });
}

auto Dir_walker::ext(std::initializer_list<std::string_view> exts) -> Dir_walker &
{
    return where([exts = std::vector<std::string>{exts.begin(), exts.end()}](const Dir_entry &entry) {
        return std::ranges::any_of(exts, [&](const auto &e) { return entry.extension() == e; });
    });
}

auto Dir_walker::named(std::string name) -> Dir_walker &
{
    return where([n = std::move(name)](const Dir_entry &entry) { return entry.filename() == n; });
}

auto Dir_walker::skip(std::string dir_name) -> Dir_walker &
{
    skips_.emplace_back(std::move(dir_name));
    return *this;
}

auto Dir_walker::skip(std::initializer_list<std::string_view> dir_names) -> Dir_walker &
{
    for (auto n : dir_names) {
        skips_.emplace_back(n);
    }
    return *this;
}

auto Dir_walker::collect(std::source_location caller) const noexcept -> Walk_result_t<std::vector<Dir_entry>>
{
    std::vector<Dir_entry> out;
    return run(
               [&](const Dir_entry &e) -> Walk_result {
                   out.push_back(e);
                   return Walk_action::next;
               },
               caller)
        .transform([&] { return std::move(out); });
}

auto Dir_walker::collect_paths(std::source_location caller) const noexcept -> Walk_result_t<std::vector<std::filesystem::path>>
{
    return collect(caller).transform(
        [](auto &&entries) { return entries | std::views::transform(&Dir_entry::path) | std::ranges::to<std::vector>(); });
}

auto Dir_walker::count(std::source_location caller) const noexcept -> Walk_result_t<std::size_t>
{
    std::size_t n = 0;
    return run(
               [&](const Dir_entry &) -> Walk_result {
                   ++n;
                   return Walk_action::next;
               },
               caller)
        .transform([&] { return n; });
}

auto Dir_walker::any(std::source_location caller) const noexcept -> Walk_result_t<bool>
{
    bool found = false;
    return run(
               [&](const Dir_entry &) -> Walk_result {
                   found = true;
                   return Walk_action::stop;
               },
               caller)
        .transform([&] { return found; });
}

auto Dir_walker::none(std::source_location caller) const noexcept -> Walk_result_t<bool>
{
    return any(caller).transform([](bool v) { return !v; });
}

auto Dir_walker::first(std::source_location caller) const noexcept -> Walk_result_t<std::optional<Dir_entry>>
{
    std::optional<Dir_entry> found;
    return run(
               [&](const Dir_entry &e) -> Walk_result {
                   found = e;
                   return Walk_action::stop;
               },
               caller)
        .transform([&] { return std::move(found); });
}

auto Dir_walker::last(std::source_location caller) const noexcept -> Walk_result_t<std::optional<Dir_entry>>
{
    std::optional<Dir_entry> found;
    return run(
               [&](const Dir_entry &e) -> Walk_result {
                   found = e;
                   return Walk_action::next;
               },
               caller)
        .transform([&] { return std::move(found); });
}

auto Dir_walker::subdir(std::string_view sub) const -> Dir_walker
{
    Dir_walker w{root_ / sub};
    w.recursive_ = recursive_;
    w.include_dirs_ = include_dirs_;
    w.include_hidden_ = include_hidden_;
    w.follow_symlinks_ = follow_symlinks_;
    w.max_depth_ = max_depth_;
    w.filters_ = filters_;
    w.skips_ = skips_;
    return w;
}

bool Dir_walker::passes_filters(const Dir_entry &e) const
{
    return std::ranges::all_of(filters_, [&](const auto &f) { return f(e); });
}

bool Dir_walker::is_skipped(const Dir_entry &e) const
{
    return e.is_dir() && std::ranges::any_of(skips_, [&](const auto &s) { return e.filename() == s; });
}

bool Dir_walker::is_visible(const Dir_entry &e) const
{
    if (!include_hidden_ && e.is_hidden()) {
        return false;
    }
    if (!include_dirs_ && e.is_dir()) {
        return false;
    }
    return true;
}

auto Dir_walker::make_entry(const std::filesystem::directory_entry &raw, int d) -> Dir_entry
{
    std::error_code ignored;
    return Dir_entry{
        .path = raw.path(),
        .type = raw.symlink_status(ignored).type(),
        .depth = d,
    };
}

auto collect(std::string_view root, std::source_location caller) noexcept -> Walk_result_t<std::vector<Dir_entry>>
{
    return Dir_walker{root}.collect(caller);
}

auto stem(std::string_view path) noexcept -> std::string
{
    return std::filesystem::path{path}.stem().string();
}

auto name(std::string_view path) noexcept -> std::string
{
    return std::filesystem::path{path}.filename().string();
}

auto extension(std::string_view path) noexcept -> std::string
{
    return std::filesystem::path{path}.extension().string();
}

auto parent_dir(std::string_view path) noexcept -> std::string
{
    return std::filesystem::path{path}.parent_path().string();
}

auto is_absolute(std::string_view path) noexcept -> bool
{
    return std::filesystem::path{path}.is_absolute();
}

auto is_relative(std::string_view path) noexcept -> bool
{
    return std::filesystem::path{path}.is_relative();
}

auto exists(std::string_view path) noexcept -> bool
{
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path{path}, ec);
}

auto is_dir(std::string_view path) noexcept -> bool
{
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path{path}, ec);
}

auto is_file(std::string_view path) noexcept -> bool
{
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path{path}, ec);
}

auto is_symlink(std::string_view path) noexcept -> bool
{
    std::error_code ec;
    return std::filesystem::is_symlink(std::filesystem::path{path}, ec);
}

auto is_empty(std::string_view path) noexcept -> std::expected<bool, bld::Err>
{
    std::error_code ec;
    bool empty = std::filesystem::is_empty(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to check if '{}' is empty", path)});
    }
    return empty;
}

auto file_size(std::string_view path) noexcept -> std::expected<std::uintmax_t, bld::Err>
{
    std::error_code ec;
    auto size = std::filesystem::file_size(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to get size of '{}'", path)});
    }
    return size;
}

auto last_write_time(std::string_view path) noexcept -> std::expected<std::filesystem::file_time_type, bld::Err>
{
    std::error_code ec;
    auto time = std::filesystem::last_write_time(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to get last write time of '{}'", path)});
    }
    return time;
}

auto copy_file(std::string_view from, std::string_view to, bool overwrite) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;

    std::filesystem::copy_file(std::filesystem::path{from}, std::filesystem::path{to}, options, ec);

    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to copy file from '{}' to '{}'", from, to)});
    }
    return {};
}

auto rename(std::string_view from, std::string_view to) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    std::filesystem::rename(std::filesystem::path{from}, std::filesystem::path{to}, ec);

    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to rename '{}' to '{}'", from, to)});
    }
    return {};
}

auto create_symlink(std::string_view target, std::string_view link) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    std::filesystem::create_symlink(std::filesystem::path{target}, std::filesystem::path{link}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to create symlink at '{}'", link)});
    }
    return {};
}

auto create_hard_link(std::string_view target, std::string_view link) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    std::filesystem::create_hard_link(std::filesystem::path{target}, std::filesystem::path{link}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to create hard link at '{}'", link)});
    }
    return {};
}

auto read_symlink(std::string_view path) noexcept -> std::expected<std::string, bld::Err>
{
    std::error_code ec;
    auto target = std::filesystem::read_symlink(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to read symlink '{}'", path)});
    }
    return target.string();
}

auto current_path() noexcept -> std::expected<std::string, bld::Err>
{
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = "Failed to get current working directory"});
    }
    return p.string();
}

auto set_current_path(std::string_view path) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    std::filesystem::current_path(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to set current path to '{}'", path)});
    }
    return {};
}

auto absolute(std::string_view path) noexcept -> std::expected<std::string, bld::Err>
{
    std::error_code ec;
    auto p = std::filesystem::absolute(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to get absolute path for '{}'", path)});
    }
    return p.string();
}

auto canonical(std::string_view path) noexcept -> std::expected<std::string, bld::Err>
{
    std::error_code ec;
    auto p = std::filesystem::canonical(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to resolve canonical path for '{}'", path)});
    }
    return p.string();
}

auto relative(std::string_view path, std::string_view base) noexcept -> std::expected<std::string, bld::Err>
{
    std::error_code ec;
    auto p = std::filesystem::relative(std::filesystem::path{path}, std::filesystem::path{base}, ec);
    if (ec) {
        return std::unexpected(bld::Err{.err = ec, .msg = std::format("Failed to resolve relative path for '{}'", path)});
    }
    return p.string();
}

auto read_file(std::string_view path) noexcept -> std::expected<std::string, bld::Err>
{
    try {

        if (!std::filesystem::is_regular_file(path)) {
            bld::log::e("Cannot read becuase '{}' is a directory.", path);
            return std::unexpected(bld::Err::erc(std::errc::io_error, std::format("'{}' is a directory", path)));
        }
        std::ifstream file(std::filesystem::path(path), std::ios::in | std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(bld::Err::erc(std::errc::io_error, std::format("Failed to open file for reading: '{}'", path)));
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string buffer;
        if (size > 0) {
            buffer.resize(static_cast<std::size_t>(size));
            file.read(buffer.data(), size);
        }
        return buffer;
    } catch (const std::exception &e) {
        return std::unexpected(bld::Err::erc(std::errc::io_error, e.what()));
    }
}

auto write_file(std::string_view path, std::string_view content) noexcept -> std::expected<void, bld::Err>
{
    try {
        std::ofstream file(std::filesystem::path(path), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file) {
            return std::unexpected(bld::Err::erc(std::errc::io_error, std::format("Failed to open file for writing: '{}'", path)));
        }
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return {};
    } catch (const std::exception &e) {
        return std::unexpected(bld::Err::erc(std::errc::io_error, e.what()));
    }
}

auto append_file(std::string_view path, std::string_view content) noexcept -> std::expected<void, bld::Err>
{
    try {
        std::ofstream file(std::filesystem::path(path), std::ios::out | std::ios::binary | std::ios::app);
        if (!file) {
            return std::unexpected(bld::Err::erc(std::errc::io_error, std::format("Failed to open file for appending: '{}'", path)));
        }
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return {};
    } catch (const std::exception &e) {
        return std::unexpected(bld::Err::erc(std::errc::io_error, e.what()));
    }
}

template <typename... Paths>
    requires(std::convertible_to<Paths, std::string_view> && ...)
[[nodiscard]] auto join(Paths &&...paths) -> std::string
{
    std::filesystem::path result;
    (..., (result /= std::filesystem::path{std::forward<Paths>(paths)}));
    return result.string();
}

template <typename... Paths>
    requires(std::convertible_to<Paths, std::string_view> && ...)
auto remove(Paths &&...paths) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    auto try_remove = [&ec](std::string_view p) -> bool {
        std::filesystem::remove_all(std::filesystem::path{p}, ec);
        return !ec;
    };
    if (!(try_remove(std::forward<Paths>(paths)) && ...)) {
        return std::unexpected(bld::Err{.err = ec, .msg = "Failed to remove one or more paths"});
    }
    return {};
}

template <typename... Paths>
    requires(std::convertible_to<Paths, std::string_view> && ...)
auto make_dirs(Paths &&...paths) noexcept -> std::expected<void, bld::Err>
{
    std::error_code ec;
    auto try_mkdir = [&ec](std::string_view p) -> bool {
        std::filesystem::create_directories(std::filesystem::path{p}, ec);
        return !ec;
    };
    if (!(try_mkdir(std::forward<Paths>(paths)) && ...)) {
        return std::unexpected(bld::Err{.err = ec, .msg = "Failed to create one or more directories"});
    }
    return {};
}

inline auto find_all_files(std::string_view root) noexcept -> Walk_result_t<std::vector<std::string>>
{
    return Dir_walker{root}.collect_paths().transform([](const auto &paths) {
        std::vector<std::string> res;
        res.reserve(paths.size());
        for (const auto &p : paths) {
            res.push_back(p.string());
        }
        return res;
    });
}

template <typename... Exts>
    requires(std::convertible_to<Exts, std::string_view> && ...)
auto find_by_ext(std::string_view root, Exts &&...exts) noexcept -> Walk_result_t<std::vector<std::string>>
{
    return Dir_walker{root}.ext({std::string_view(exts)...}).collect_paths().transform([](const auto &paths) {
        std::vector<std::string> res;
        res.reserve(paths.size());
        for (const auto &p : paths) {
            res.push_back(p.string());
        }
        return res;
    });
}

template <typename... Names>
    requires(std::convertible_to<Names, std::string_view> && ...)
auto find_by_name(std::string_view root, Names &&...names) noexcept -> Walk_result_t<std::vector<std::string>>
{
    std::vector<std::string_view> targets{std::forward<Names>(names)...};
    return Dir_walker{root}
        .where([targets](const Dir_entry &e) { return std::ranges::find(targets, e.filename()) != targets.end(); })
        .collect_paths()
        .transform([](const auto &paths) {
            std::vector<std::string> res;
            res.reserve(paths.size());
            for (const auto &p : paths) {
                res.push_back(p.string());
            }
            return res;
        });
}

} // namespace bld::fs

namespace bld::str {

auto trim_left(std::string_view s) noexcept -> std::string_view
{
    auto it = std::ranges::find_if_not(s, [](unsigned char c) { return std::isspace(c); });
    return s.substr(static_cast<std::size_t>(std::distance(s.begin(), it)));
}

auto trim_right(std::string_view s) noexcept -> std::string_view
{
    auto it = std::ranges::find_if_not(s | std::views::reverse, [](unsigned char c) { return std::isspace(c); });
    return s.substr(0, s.size() - static_cast<std::size_t>(std::distance(s.rbegin(), it)));
}

auto trim(std::string_view s) noexcept -> std::string_view
{
    return trim_right(trim_left(s));
}

auto split(std::string_view s, char delimiter) -> std::vector<std::string_view>
{
    std::vector<std::string_view> result;
    std::size_t start = 0;
    std::size_t end = s.find(delimiter);

    while (end != std::string_view::npos) {
        result.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delimiter, start);
    }
    result.push_back(s.substr(start));
    return result;
}

auto split(std::string_view s, std::string_view delimiter) -> std::vector<std::string_view>
{
    std::vector<std::string_view> result;
    if (delimiter.empty()) {
        result.push_back(s);
        return result;
    }

    std::size_t start = 0;
    std::size_t end = s.find(delimiter);

    while (end != std::string_view::npos) {
        result.push_back(s.substr(start, end - start));
        start = end + delimiter.size();
        end = s.find(delimiter, start);
    }
    result.push_back(s.substr(start));
    return result;
}

auto to_lower(std::string_view s) -> std::string
{
    std::string result(s);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

auto to_upper(std::string_view s) -> std::string
{
    std::string result(s);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

auto replace_all(std::string_view s, std::string_view from, std::string_view to) -> std::string
{
    if (from.empty()) {
        return std::string(s);
    }

    std::string result;
    std::size_t pos = 0;
    std::size_t last = 0;

    while ((pos = s.find(from, last)) != std::string_view::npos) {
        result.append(s.data() + last, pos - last);
        result.append(to);
        last = pos + from.size();
    }
    result.append(s.data() + last, s.size() - last);
    return result;
}

auto parse_int(std::string_view s, int base) noexcept -> std::expected<int, bld::Err>
{
    int value = 0;
    auto trimmed = trim(s);
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value, base);

    if (ec == std::errc() && ptr == trimmed.data() + trimmed.size()) {
        return value;
    }
    return std::unexpected(bld::Err::erc(std::errc::invalid_argument, std::format("Failed to parse int from '{}'", s)));
}

auto parse_double(std::string_view s) noexcept -> std::expected<double, bld::Err>
{
    double value = 0.0;
    auto trimmed = trim(s);
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);

    if (ec == std::errc() && ptr == trimmed.data() + trimmed.size()) {
        return value;
    }
    return std::unexpected(bld::Err::erc(std::errc::invalid_argument, std::format("Failed to parse double from '{}'", s)));
}

auto parse_bool(std::string_view s) noexcept -> std::expected<bool, bld::Err>
{
    auto t = to_lower(trim(s));
    if (t == "true" || t == "1" || t == "yes" || t == "y") {
        return true;
    }
    if (t == "false" || t == "0" || t == "no" || t == "n") {
        return false;
    }
    return std::unexpected(bld::Err::erc(std::errc::invalid_argument, std::format("Failed to parse bool from '{}'", s)));
}

template <std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
[[nodiscard]] auto join(const Range &range, std::string_view delimiter) -> std::string
{
    std::string result;
    bool first = true;
    for (const auto &item : range) {
        if (!first) {
            result.append(delimiter);
        }
        first = false;
        result.append(std::string_view{item});
    }
    return result;
}

} // namespace bld::str
namespace bld::time {

auto stamp::reset() noexcept -> std::chrono::nanoseconds
{
    auto now = clock_t::now();
    auto diff = now - tp_;
    tp_ = now;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(diff);
}

auto stamp::elapsed() const noexcept -> std::chrono::nanoseconds
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(clock_t::now() - tp_);
}

auto stamp::since(const stamp &baseline) const noexcept -> std::chrono::nanoseconds
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tp_ - baseline.tp_);
}

auto now() noexcept -> stamp
{
    return stamp{};
}

auto since(const stamp &baseline) noexcept -> std::chrono::nanoseconds
{
    return baseline.elapsed();
}

auto format(std::chrono::nanoseconds ns) -> std::string
{
    if (ns.count() >= 1'000'000'000) {
        double s = static_cast<double>(ns.count()) / 1'000'000'000.0;
        return std::format("{:.3f}s", s);
    }
    if (ns.count() >= 1'000'000) {
        double ms = static_cast<double>(ns.count()) / 1'000'000.0;
        return std::format("{:.2f}ms", ms);
    }
    if (ns.count() >= 1'000) {
        double us = static_cast<double>(ns.count()) / 1'000.0;
        return std::format("{:.1f}us", us);
    }
    return std::format("{}ns", ns.count());
}

} // namespace bld::time

#endif // B_LDR_IMPLEMENTATION_ONCE
#endif // B_LDR_IMPLEMENTATION
