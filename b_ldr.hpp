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

/*
  HEAVILY INSPIRED BY nob.h by rexim/alexey/tsoding.
  github.com/tsoding/nob.h
*/

#include <any>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <ostream>
#include <print>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

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

    // clang-format off
    struct Default_logger_fn
    {
        inline static Level min_lvl = Level::inf;
        inline static log::detail::Stream_proxy ostream;

        auto operator()(std::ostream &stream, const Log_record &record) const -> void
        {
            if (record.lvl < this->min_lvl) {
                return;
            }
            std::string_view prefix;
            switch (record.lvl) {
            case Level::dbg: prefix = "[DEBUG]"; break;
            case Level::inf: prefix = "[INFO]"; break;
            case Level::wrn: prefix = "[WARN]"; break;
            case Level::err: prefix = "[ERROR]"; break;
            case Level::ftl: prefix = "[FATAL]"; break;
            }
            std::println(stream, "{}: {}", prefix, record.str);
        };
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
    invoke_logger(bld::Logger::Level::inf, bld::Logger::Default_logger_fn::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void w(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::wrn, bld::Logger::Default_logger_fn::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void e(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::err, bld::Logger::Default_logger_fn::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void d(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::dbg, bld::Logger::Default_logger_fn::ostream.get(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void f(std::format_string<Args...> fmt, Args &&...args)
{
    invoke_logger(bld::Logger::Level::ftl, bld::Logger::Default_logger_fn::ostream.get(), fmt, std::forward<Args>(args)...);
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

    Proc() = default;

    Proc(P_id id, const std::string &label_ = "") : id_(id)
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
            kill(SIGKILL);
            std::ignore = wait();
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
}; // namespace bld

namespace bld {

}; // namespace bld

#ifdef B_LDR_IMPLEMENTATION

#include <filesystem>

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
#endif // B_LDR_IMPLEMENTATION
