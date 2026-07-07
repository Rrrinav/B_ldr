/*
  MIT
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

/*
  inspired by nob.h by rexim/alexey/tsoding.
  github.com/tsoding/nob.h
*/

/*
Usage:
#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"
 */

#include <any>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <flat_map>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <ostream>
#include <print>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Linux
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>

// clang-format off
namespace bld {
struct Err
{
    using Error_pt = std::shared_ptr<Err>;
    std::error_code err;
    std::string msg{""};
    std::any payload{};
    Error_pt cause_{nullptr};

    static auto erc(std::errc code, std::string message = "") -> Err { return Err{.err = std::make_error_code(code), .msg = std::move(message)}; }
    static auto erno(int code, std::string message = "") -> Err { return Err{.err = std::error_code(code, std::generic_category()), .msg = std::move(message)}; }
    template <typename T>
    auto with_payload(T &&data) && -> Err { payload = std::forward<T>(data); return std::move(*this); }
    auto with_cause(Err root_cause) && -> Err { cause_ = std::make_shared<Err>(std::move(root_cause)); return std::move(*this); }
    auto with_cause(Error_pt root_cause_ptr) && -> Err { cause_ = std::move(root_cause_ptr); return std::move(*this); }
};
}; // namespace bld

template <>
struct std::formatter<bld::Err>
{
    enum class mode { plain, debug };
    mode fmt = mode::plain;
    constexpr auto parse(std::format_parse_context &ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            switch (*it) {
            case '?': fmt = mode::debug; break;
            case 'p': fmt = mode::plain; break;
            default: throw std::format_error("invalid Cmd format");
            }
            ++it;
        }
        return it;
    }
    auto format(const bld::Err &err, std::format_context &ctx) const
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
        if (err.cause_) { out = std::format_to(out, "\n      -> caused by: {}", *err.cause_); }
        return out;
    }
};
// clang-format on

// Logger
namespace bld::log::detail {
// A proxy for easier handling of streams.
// clang-format off
struct Stream_proxy
{
    std::ostream *ptr = &std::cerr;
    void operator=(std::ostream &os) { ptr = &os; }
    std::ostream &get() const { return *ptr; }
};
// clang-format on
} // namespace bld::log::detail

namespace bld {
// We dont need much performance here in this logging, so this is a simple design
// with very simple API.
//
// Mostly configuration and not actually the "real" logger
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
        static constexpr auto style(Level lvl) noexcept -> Style
        {
            using namespace std::string_view_literals;
            switch (lvl) {
            case Level::dbg: return {"[DEBUG]", "\x1b[38;2;120;170;255m"sv};
            case Level::inf: return {"[INFO] ", "\x1b[38;2;0;200;120m"sv};
            case Level::wrn: return {"[WARN] ", "\x1b[38;2;255;180;0m"sv};
            case Level::err: return {"[ERROR]", "\x1b[38;2;255;64;64m"sv};
            case Level::ftl: return {"[FATAL]", "\x1b[38;2;200;0;0m"sv};
            }
            std::unreachable();
        }

        auto operator()(std::ostream& stream, const Log_record& record) const -> void
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
    };
    // clang-format on

    inline static std::atomic<Level> min_lvl{Level::inf};

    // Throws
    static auto set_logger_fn(Logger_fn_t fn, std::source_location loc = std::source_location::current())
    {
        std::lock_guard guard(config_mtx);
        // Improved: use .load() for atomics
        if (logger_locked.load()) {
            throw std::runtime_error{
                std::format("{}:{}:{}: err: Logger already set, you cannot set it twice", loc.file_name(), loc.line(), loc.column())};
        }
        logger_locked = true;
        logger_fn = std::move(fn);
    }

    // Changing this thing is not thread safe because a function maybe being used to log, in another thread.
    // So, I am enforcing that you can only set it once before execution starts.
    // Use the function 'set_logger_fn' to set the logger, please.
    //
    // If you know what you are doing and want to set it again, you can still use these public members
    inline static std::atomic<bool> logger_locked{false};
    inline static Logger_fn_t logger_fn = Default_logger_fn{};
    inline static std::mutex config_mtx;
};
} // namespace bld

// Real logging functions
// Instead of passing aroung enums, you can just:
// bld::log::i(fmt, args); for info and so on for error, debug, warn and fatal using: e, d, f respectively
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

// explicit stream overloads
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

// Command abstraction
namespace bld {
// I would have used std::string but then exposing args_ means
// everyone has to keep track of spaces " ", which is tedious.
struct Cmd
{
    using value_type = std::vector<std::string>;
    using const_iterator = std::vector<std::string>::const_iterator;
    // clang-format off
    std::vector<std::string> args_;

    Cmd() = default;

    template <typename... Ts>
        requires(std::convertible_to<Ts, std::string_view> && ...)
    explicit Cmd(Ts &&...ts) { args_.reserve(sizeof...(Ts)); (args_.emplace_back(std::forward<Ts>(ts)), ...); }

    auto push(std::string_view s) -> void { args_.emplace_back(s); }

    template <typename... Ts>
    auto emplace_b(Ts &&...ts) -> std::string & { return args_.emplace_back(std::forward<Ts>(ts)...); }
    template <typename... Ts>
    auto emplace(const_iterator it, Ts &&...ts) -> std::string &   { return args_.emplace(it, std::forward<Ts>(ts)...); }
    auto begin(this auto &self)       noexcept { return self.args_.begin(); }
    auto end(this auto &self)         noexcept { return self.args_.end(); }
    auto size(this auto const &self)  noexcept { return self.args_.size(); }
    auto empty(this auto const &self) noexcept { return self.args_.empty(); }
    auto span(this auto const &self)  noexcept { return std::span{self.args_}; }

    auto argv() const -> std::vector<char*> {
        auto out = args_
                 | std::views::transform([](std::string const& s) { return const_cast<char*>(s.c_str()); })
                 | std::ranges::to<std::vector>();
        out.push_back(nullptr);
        return out;
    }

    [[nodiscard]]
    auto str() const -> std::string { return args_ | std::views::join_with(std::string_view{" "}) | std::ranges::to<std::string>(); }
    auto reset() { args_.clear(); }
    // clang-format on
};
} // namespace bld

// clang-format off
template <>
struct std::formatter<bld::Cmd>
{
    enum class mode { plain, unquoted, debug };
    mode fmt = mode::plain;
    constexpr auto parse(std::format_parse_context &ctx)
    {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            switch (*it) {
            case 'q': fmt = mode::unquoted; break;
            case '?': fmt = mode::debug;    break;
            case 'p': fmt = mode::plain;    break;
            default: throw std::format_error("invalid Cmd format");
            }
            ++it;
        }
        return it;
    }
    auto format(const bld::Cmd &cmd, std::format_context &ctx) const
    {
        auto out = ctx.out();
        switch (fmt) {
        case mode::plain:    out = std::format_to(out, "\"{}\"", cmd.str()); break;
        case mode::unquoted: out = std::format_to(out, "{}", cmd.str());     break;
        case mode::debug:    out = std::format_to(out, "{}", cmd.args_);     break;
        }
        return out;
    }
};
// clang-format on

// Process
namespace bld {

struct Proc
{
    using P_id = pid_t;
    enum class State : ::std::uint8_t { running, exited, signaled, stopped, continued };

    struct Status
    {
        State state{State::exited};
        ::std::uint8_t code{EXIT_SUCCESS};
    };

    P_id id_{-1};
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

    explicit Proc(P_id id, const std::string &label_ = "") : id_(id)
    {
        if (label_.empty()) {
            constexpr ::std::size_t size{12};
            char buf[size];
            if (auto [end, ec] = std::to_chars(buf, buf + size, id); ec == std::errc{}) {
                label = std::string{buf, end};
            }
        } else {
            label = label_;
        }
        status_ = Status{.state = State::running};
    }

    // Absolute zero-leak guarantee
    ~Proc()
    {
        if (id_ > 0 && status_.state == State::running) {
            this->kill(SIGKILL);
            std::ignore = this->wait();
        }
    }

    // Rule of 5: Prevent accidental zombie clones
    Proc(const Proc &) = delete;
    Proc &operator=(const Proc &) = delete;

    Proc(Proc &&other) noexcept : id_(std::exchange(other.id_, -1)), status_(other.status_), label(std::move(other.label))
    {}

    Proc &operator=(Proc &&other) noexcept
    {
        if (this != &other) {
            if (id_ > 0 && status_.state == State::running) {
                kill(SIGKILL);
                std::ignore = wait();
            }
            id_ = std::exchange(other.id_, -1);
            status_ = other.status_;
            label = std::move(other.label);
        }
        return *this;
    }

    [[nodiscard]]
    auto wait() -> std::expected<Status, bld::Err>
    {
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
    }

    [[nodiscard]]
    auto try_wait() -> std::expected<Status, bld::Err>
    {
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
    }

    auto kill(int sig = SIGTERM) -> void
    {
        if (id_ > 0 && status_.state == State::running) {
            ::kill(id_, sig);
        }
    }

    // clang-format off
    [[nodiscard]] auto pid() const -> P_id        { return id_; }
    [[nodiscard]] auto status() const -> Status   { return status_; }
    [[nodiscard]] auto is_running() const -> bool { return status_.state == State::running; }
    [[nodiscard]] auto status_code() const -> int { return +status_.code; }
    // clang-format on

    [[nodiscard]]
    static auto spawn(const Cmd &cmd, const std::string &label_ = "", const Io_routing &io = Default_route) -> std::expected<Proc, bld::Err>
    {
        if (cmd.empty()) {
            return std::unexpected(bld::Err::erc(std::errc::invalid_argument, "Command cannot be empty"));
        }
        P_id pid = ::fork();
        if (pid < 0) {
            return std::unexpected(bld::Err::erno(errno, "Fork failed").with_payload(cmd));
        }
        if (pid == 0) {
            // child
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
            // 4. Close original FDs to prevent leaks into the exec image.
            // We strictly check > 2 to ensure we don't accidentally close terminal streams.
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
    }

    [[nodiscard]]
    static auto wait_pid(P_id pid, int options = 0) -> std::expected<Status, bld::Err>
    {
        int wstatus = 0;
        P_id res = ::waitpid(pid, &wstatus, options);

        if (res == -1) {
            if (errno == ECHILD) {
                return Status{.state = State::exited, .code = 255};
            }
            return std::unexpected(bld::Err::erno(errno, "waitpid failed"));
        }

        if (res == 0) {
            return Status{.state = State::running, .code = 0};
        }

        return parse_status(wstatus);
    }

    [[nodiscard]]
    static auto try_wait_pid(P_id pid) -> std::expected<Status, bld::Err>
    {
        return wait_pid(pid, WNOHANG);
    }

    static auto parse_status(int wstatus) -> Status
    {
        Status s{};
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
        return s;
    }

private:
    auto update_status(int wstatus) -> void
    {
        status_ = parse_status(wstatus);
    }
};
}; // namespace bld

// clang-format off
template <>
struct std::formatter<bld::Proc::Status>
{
    enum class mode { plain, debug };
    mode fmt = mode::plain;

    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();

        if (it != ctx.end() && *it != '}') {
            switch (*it) {
            case 'p': fmt = mode::plain; break;
            case '?': fmt = mode::debug; break;
            default: throw std::format_error("invalid Proc::Status format specifier");
            }
            ++it;
        }
        return it;
    }
    auto format(const bld::Proc::Status& s, std::format_context& ctx) const
    {
        auto out = ctx.out();
        switch (s.state) {
        case bld::Proc::State::running: out = std::format_to(out, "{{ running"); break;
        case bld::Proc::State::exited: out = std::format_to(out, "{{ exited"); break;
        case bld::Proc::State::signaled: out = std::format_to(out, "{{ signaled"); break;
        case bld::Proc::State::stopped: out = std::format_to(out, "{{ stopped"); break;
        case bld::Proc::State::continued: out = std::format_to(out, "{{ continued"); break;
        default: std::unreachable();
        }
        switch (s.state) {
        case bld::Proc::State::exited:
            if (fmt == mode::debug) {
                out = std::format_to(out, ", code = {} }}", +s.code);
            } else {
                out = std::format_to(out, ", {} }}", +s.code);
            } break;
        case bld::Proc::State::signaled:
            if (fmt == mode::debug) {
                out = std::format_to(out, ", sig = {} }}", +s.code);
            } else {
                out = std::format_to(out, ", {} }}", +s.code);
            } break;
        case bld::Proc::State::running:
        case bld::Proc::State::stopped:
        case bld::Proc::State::continued: out = std::format_to(out, " }}"); break;
        default: std::unreachable();
        }

        return out;
    }
};
template <>
struct std::formatter<bld::Proc>
{
    bool show_pid   = false;
    bool show_debug = false;
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();

        while (it != ctx.end() && *it != '}') {
            switch (*it) {
            case 'p': show_pid = true; break;
            case '?': show_debug = true; break;
            default: throw std::format_error("invalid Proc format specifier");
            }
            ++it;
        }
        return it;
    }
    auto format(const bld::Proc& p, std::format_context& ctx) const
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
};
// clang-format on

// execute api
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
    explicit constexpr Fd_view(Native_t v) : val(v)
    {}

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
    {
        return val != INVALID;
    }
};

struct Owned_Fd
{
    Fd_view::Native_t handle_{Fd_view::INVALID};

    constexpr Owned_Fd() = default;
    explicit constexpr Owned_Fd(Fd_view::Native_t v) : handle_(v)
    {}

    // RAII Guarantee
    ~Owned_Fd()
    {
        close();
    }

    // Rule of 5: Move-only semantics prevent double-closing!
    Owned_Fd(const Owned_Fd &) = delete;
    Owned_Fd &operator=(const Owned_Fd &) = delete;

    Owned_Fd(Owned_Fd &&other) noexcept : handle_(std::exchange(other.handle_, Fd_view::INVALID))
    {}
    Owned_Fd &operator=(Owned_Fd &&other) noexcept
    {
        if (this != &other) {
            close();
            handle_ = std::exchange(other.handle_, Fd_view::INVALID);
        }
        return *this;
    }

    auto close() -> void
    {
        if (handle_ != Fd_view::INVALID && handle_ != Fd_view::DEFAULT_IN && handle_ != Fd_view::DEFAULT_OUT && handle_ != Fd_view::DEFAULT_ERR) {
            ::close(handle_);
            handle_ = Fd_view::INVALID;
        }
    }

    operator Fd_view() const
    {
        return Fd_view{handle_};
    }

    [[nodiscard]]
    static auto open(std::string_view path, Open_mode mode = Open_mode::read) -> std::expected<Owned_Fd, bld::Err>
    {
        if (path.empty()) {
            return std::unexpected(bld::Err::erc(std::errc::invalid_argument, "Cannot open an empty path"));
        }
        int flags = 0;
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
        if (fd == Fd_view::INVALID) {
            return std::unexpected(bld::Err::erno(errno, std::format("Failed to open file: '{}'", path)));
        }
        return Owned_Fd{fd};
    }
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

// DECLARE THE HEAVY CAPTURE BACKEND
auto capture_execute(const bld::Cmd &cmd, bld::Capture_config &cap_cfg, std::source_location loc = std::source_location::current())
    -> std::expected<bld::Proc::Status, bld::Err>;
}; // namespace details

// clang-format off
struct async
{
    auto operator()(Proc_config &cfg) const -> void { cfg.async = true; }
};
struct label
{
    std::string val{""};
    auto operator()(bld::Proc_config &cfg) const -> void { cfg.label = val; }
    auto operator()(bld::Capture_config &cfg) const -> void { cfg.label = val; }
};
struct pipe
{
    Fd_view out{Fd_view::DEFAULT_OUT}, in{Fd_view::DEFAULT_IN}, err{Fd_view::DEFAULT_ERR};
    bool merge_out_err{false};
    auto operator()(Proc_config& cfg) const -> void { cfg.out = out; cfg.err = err; cfg.in  = in; cfg.merge_err_and_out = false; }
};
struct out_s
{
    Fd_view fd{Fd_view::INVALID};
    auto operator()(bld::Proc_config& cfg) const -> void { cfg.out = fd; }
};
struct err_s
{
    Fd_view fd{Fd_view::INVALID};
    auto operator()(bld::Proc_config& cfg) const -> void { cfg.err = fd; }
};
struct in_s
{
    Fd_view fd{Fd_view::INVALID};
    auto operator()(bld::Proc_config& cfg) const -> void { cfg.in = fd; }
    auto operator()(bld::Capture_config& cfg) const -> void { cfg.in = fd; }
};
// When you do bld::run(....., bld::out_f{"file"});
// The fork() will happen and Fd ref-count will go up in the OS and hence
// the Fd will always stay active even when the function is finished in the parent and
// and destructor of Owned_Fd will only reduce the ref-count and not totally close the Fd.
//
// So as long as you are using these functions with fork, we are fine.
struct out_f
{
    bld::Owned_Fd fd{};
    out_f(std::string_view path, bld::Open_mode mode = bld::Open_mode::write);
    auto operator()(bld::Proc_config& cfg) const -> void { cfg.out = fd; }
};
struct err_f
{
    bld::Owned_Fd fd{};
    err_f(std::string_view path, bld::Open_mode mode = bld::Open_mode::write);
    auto operator()(bld::Proc_config& cfg) const -> void { cfg.err = fd; }
};
struct in_f
{
    bld::Owned_Fd fd{};
    in_f(std::string_view path);
    auto operator()(bld::Proc_config& cfg) const -> void { cfg.in = fd; }
    auto operator()(bld::Capture_config& cfg) const -> void { cfg.in = fd; }
};
struct cap_out
{
    std::string* ptr{nullptr};
    explicit cap_out(std::string& s) : ptr(&s) {}
    auto operator()(Capture_config& cfg) const -> void { cfg.out = ptr; }
};
struct cap_err
{
    std::string* ptr{nullptr};
    explicit cap_err(std::string& s) : ptr(&s) {}
    auto operator()(Capture_config& cfg) const -> void { cfg.err = ptr; }
};
struct cap_merge
{
    std::string* ptr{nullptr};
    explicit cap_merge(std::string& s) : ptr(&s) {}
    auto operator()(Capture_config& cfg) const -> void { cfg.out = ptr; cfg.merge_out_err = true; }
};
struct in_str
{
    std::string_view val;
    explicit in_str(std::string_view s) : val(s) {}
    auto operator()(Capture_config& cfg) const -> void { cfg.in_str = val; }
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

struct Cmd_loc
{
    const Cmd &cmd;
    std::source_location loc;

    Cmd_loc(const Cmd &c, std::source_location l = std::source_location::current()) : cmd(c), loc(l)
    {}
};

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

template <typename T>
concept Capture_modifier_c = requires(T &&modifier, Capture_config &cfg) { modifier(cfg); };

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

auto wait_all(std::span<bld::Proc> procs) -> std::expected<std::size_t, bld::Err>;

struct Task
{
    bld::Cmd cmd;
    bld::Proc_config cfg;

    template <typename... Configs>
        requires(Config_modifier_c<Configs> && ...)
    explicit Task(Cmd_loc cl, Configs &&...confs) : cmd(cl.cmd)
    {
        bld::validate_run_configs<Configs...>();
        (confs(cfg), ...);
        cfg.loc = cl.loc;
        cfg.async = true;
    }
};
auto run(std::span<bld::Task> tasks, std::size_t max_jobs = 0) -> std::expected<void, bld::Err>;
}; // namespace bld

namespace bld {
[[nodiscard]]
auto is_outdated(std::string_view target, std::string_view source) -> bool;

template <std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
[[nodiscard]] auto is_outdated(std::string_view target, const Range &sources) -> bool;

auto get_current_cxx_compiler() -> std::string_view;
auto rebuild_this_when_needed(int argc, char **argv, std::string_view compiler = "", std::source_location loc = std::source_location::current())
    -> void;
// Also considers changes in this header file.
auto rebuild_this_when_needed_ext(
    int argc,
    char **argv,
    std::vector<std::string> flags = {},
    std::string_view compiler = "",
    std::source_location loc = std::source_location::current()
) -> void;

}; // namespace bld

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

    static auto get() -> Config &
    {
        static Config instance;
        return instance;
    }

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

    auto operator[](std::string_view key) const -> Proxy
    {
        return Proxy{this, key};
    }

    auto print_help(std::string_view prog_name, std::string_view specific_opt = "") const -> void;

    auto parse(int argc, char *argv[]) -> void;

    template <typename T>
    auto get_val(std::string_view key) const -> std::expected<T, bld::Err>;

private:
    Config() = default;
};

template <typename T>
inline auto Config::get_val(std::string_view key) const -> std::expected<T, bld::Err>
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

template <>
struct std::formatter<std::unordered_map<std::string_view, bld::Config::value_type>>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        auto it = ctx.begin();
        return it;
    }
    auto format(const std::unordered_map<std::string_view, bld::Config::value_type> &m, std::format_context &ctx) const
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
    operator bool() const { return same; }
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

    constexpr auto parse(std::format_parse_context &ctx)
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
                throw std::format_error("invalid format specifier for compute_diff_op");
            }
        }

        if (it != end && *it != '}') {
            throw std::format_error("invalid format specifier for compute_diff_op");
        }

        return it;
    }

    auto format(const bld::test::compute_diff_op &diff, std::format_context &ctx) const
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
};

namespace bld {

auto make_dir_if_not_exists(std::string_view path, bool create_parents = true, std::source_location loc = std::source_location::current()) noexcept
    -> bool;

};

#ifdef B_LDR_IMPLEMENTATION

#include <cstring>
#include <filesystem>
#include <thread>

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

auto bld::details::capture_execute(const bld::Cmd &cmd, bld::Capture_config &cap_cfg, std::source_location loc)
    -> std::expected<bld::Proc::Status, bld::Err>
{
    bld::log::i("Setting up capture for cmd: {:?}", cmd);

    auto make_pipe = [](int p[2], const char *name) -> std::expected<void, bld::Err> {
        if (::pipe(p) == -1) {
            return std::unexpected(bld::Err::erno(errno, std::format("{} pipe failed", name)));
        }
        ::fcntl(p[0], F_SETFD, FD_CLOEXEC);
        ::fcntl(p[1], F_SETFD, FD_CLOEXEC);
        return {};
    };

    int pipe_out[2]{-1, -1}, pipe_err[2]{-1, -1}, pipe_in[2]{-1, -1};
    Proc_config run_cfg{.label = cap_cfg.label, .async = true, .loc = cap_cfg.loc, .in = cap_cfg.in};

    if (!cap_cfg.in_str.empty()) {
        if (auto res = make_pipe(pipe_in, "stdin"); !res) {
            return std::unexpected(res.error());
        }
        run_cfg.in = Fd_view{pipe_in[0]};
        bld::log::i("Created stdin pipe (read: {}, write: {})", pipe_in[0], pipe_in[1]);
    }

    if (cap_cfg.out) {
        if (auto res = make_pipe(pipe_out, "stdout"); !res) {
            return std::unexpected(res.error());
        }
        run_cfg.out = Fd_view{pipe_out[1]};
        bld::log::i("Created stdout pipe (read: {}, write: {})", pipe_out[0], pipe_out[1]);
    }

    if (cap_cfg.merge_out_err) {
        run_cfg.merge_err_and_out = true;
        bld::log::i("Merging stderr into stdout pipe");
    } else if (cap_cfg.err) {
        if (auto res = make_pipe(pipe_err, "stderr"); !res) {
            return std::unexpected(res.error());
        }
        run_cfg.err = Fd_view{pipe_err[1]};
        bld::log::i("Created stderr pipe (read: {}, write: {})", pipe_err[0], pipe_err[1]);
    }

    auto proc_res = bld::details::execute(cmd, run_cfg, loc);

    if (cap_cfg.out) {
        ::close(pipe_out[1]);
    }
    if (cap_cfg.err && !cap_cfg.merge_out_err) {
        ::close(pipe_err[1]);
    }
    if (!cap_cfg.in_str.empty()) {
        ::close(pipe_in[0]);
    }

    if (!proc_res) {
        return std::unexpected(proc_res.error());
    }
    auto &proc = *proc_res;

    struct ::pollfd fds[3];
    int nfds = 0;

    if (cap_cfg.out) {
        fds[nfds++] = {.fd = pipe_out[0], .events = POLLIN, .revents = 0};
    }
    if (cap_cfg.err && !cap_cfg.merge_out_err) {
        fds[nfds++] = {.fd = pipe_err[0], .events = POLLIN, .revents = 0};
    }
    if (!cap_cfg.in_str.empty()) {
        fds[nfds++] = {.fd = pipe_in[1], .events = POLLOUT, .revents = 0};
    }

    std::size_t written = 0;

    while (nfds > 0) {
        if (::poll(fds, static_cast<::nfds_t>(nfds), -1) == -1) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(bld::Err::erno(errno, "poll failed"));
        }

        for (int i = 0; i < nfds; ++i) {
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                if (fds[i].fd == pipe_out[0] || fds[i].fd == pipe_err[0]) {
                    char buf[4096];
                    ssize_t bytes = ::read(fds[i].fd, buf, sizeof(buf));

                    if (bytes > 0) {
                        if (fds[i].fd == pipe_out[0]) {
                            cap_cfg.out->append(buf, static_cast<std::size_t>(bytes));
                        } else {
                            cap_cfg.err->append(buf, static_cast<std::size_t>(bytes));
                        }
                    } else if (bytes == 0) {
                        ::close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        nfds--;
                        i--;
                        continue;
                    }
                }
            }

            if (fds[i].revents & POLLOUT) {
                if (fds[i].fd == pipe_in[1]) {
                    ssize_t bytes = ::write(fds[i].fd, cap_cfg.in_str.data() + written, cap_cfg.in_str.size() - written);

                    if (bytes > 0) {
                        written += static_cast<std::size_t>(bytes);
                        if (written == cap_cfg.in_str.size()) {
                            bld::log::i("Finished writing input to child, sending EOF");
                            ::close(fds[i].fd);
                            fds[i] = fds[nfds - 1];
                            nfds--;
                            i--;
                            continue;
                        }
                    } else if (bytes == -1 && errno != EINTR && errno != EAGAIN) {
                        ::close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        nfds--;
                        i--;
                        continue;
                    }
                }
            }
        }
    }

    return proc.wait();
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

template <std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
auto bld::is_outdated(std::string_view target, const Range &sources) -> bool
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

    auto proc = bld::run(build_cmd);
    if (!proc || proc->status_.code != 0) {
        bld::log::e("FATAL: Failed to rebuild build script.");
        fs::rename(old_target, target, ec);
        std::exit(1);
    }

    bld::log::i("Successfully rebuilt! Restarting...");

    // 5. The Restart
    std::vector<char *> c_argv(args_span.begin(), args_span.end());
    c_argv.push_back(nullptr);

    ::execvp(target.c_str(), c_argv.data());

    bld::log::f("FATAL: Failed to restart build script after compilation: {}", std::strerror(errno));
    std::exit(1);
}

auto bld::rebuild_this_when_needed_ext(int argc, char **argv, std::vector<std::string> flags, std::string_view compiler, std::source_location loc) -> void
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
        // -1 to block this thread until any child process exits
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
                continue; // Interrupted by signal, just retry
            }
            if (errno == ECHILD) {
                break; // No more child processes exist
            }
            bld::log::e("Error in waiting for procs.");
            return std::unexpected(bld::Err::erno(errno, "waitpid failed in wait_all").with_payload(completed));
        }
    }

    if (has_errors) {
        return std::unexpected(bld::Err::erc(std::errc::operation_canceled, "One or more async processes failed").with_payload(completed));
    }

    return completed;
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
        // Wait until we have a free CPU core!
        if (active_procs.size() >= max_jobs) {
            auto wait_res = bld::wait_all(active_procs);
            if (!wait_res) {
                // FIX 1: Added std::any_cast
                completed += std::any_cast<std::size_t>(wait_res.error().payload);
                bld::log::i("Waiting failed; total completed tasks: {}", completed);

                // FIX 3: Update the payload to reflect the TOTAL completed across all batches
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

bld::test::Frontier::Frontier(std::ptrdiff_t max_d) : data(2 * max_d + 1, 0), offset(max_d)
{}
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

    // 1. Forward Pathfinding
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

    // 2. Backtrack to extract the shortest path
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

auto bld::make_dir_if_not_exists(std::string_view path, bool create_parents, std::source_location loc) noexcept -> bool
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

#endif // B_LDR_IMPLEMENTATION
