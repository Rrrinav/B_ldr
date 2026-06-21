#include <algorithm>
#include <cctype>
#include <format>
#include <string_view>
#include <variant>
#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"

#include <flat_map>
#include <unordered_map>

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
        -> Config &
    {
        options[flag] = Option{type, std::string(desc), std::move(def), std::move(valid_choices)};
        return *this;
    }

    // THE PROXY OBJECT
    struct Proxy
    {
        const Config *cfg;
        std::string_view key;

        // Allows: if (cfg["kk"])
        operator bool() const
        {
            auto it = cfg->data.find(key);
            if (it == cfg->data.end()) {
                return false;
            }
            // If it is explicitly a boolean, return its true/false state
            if (auto *b = std::get_if<bool>(&it->second)) {
                return *b;
            }
            // If it exists and is NOT a bool (e.g. an int or string), it evaluates to true
            return true;
        }

        operator std::string() const
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

        operator int() const
        {
            if (auto res = cfg->get_val<int>(key)) {
                return *res;
            } else {
                throw std::runtime_error(std::format("Config error: {}", res.error().msg));
            }
        }

        operator double() const
        {
            if (auto res = cfg->get_val<double>(key)) {
                return *res;
            } else {
                throw std::runtime_error(std::format("Config error: {}", res.error().msg));
            }
        }

        operator std::vector<std::string>() const
        {
            if (auto res = cfg->get_val<std::vector<std::string>>(key)) {
                return *res;
            } else {
                throw std::runtime_error(std::format("Config error: {}", res.error().msg));
            }
        }
    };

    // Syntactic sugar accessor
    auto operator[](std::string_view key) const -> Proxy
    {
        return Proxy{this, key};
    }

    auto print_help(std::string_view prog_name, std::string_view specific_opt = "") const -> void
    {
        std::string help_text;

        // Specific Help Trigger
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

        // Fallback to General Help
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

    auto parse(int argc, char *argv[]) -> void
    {
        std::span<char *> args{argv, static_cast<std::size_t>(argc)};
        std::string_view prog_name = args.empty() ? "bld" : args[0];

        // 1. Pre-scan for Help requests
        for (auto i{1uz}; i < static_cast<std::size_t>(argc); ++i) {
            std::string_view curr = args[i];
            if (curr == "-h" || curr == "--help") {
                std::string_view specific = "";
                // Check if they typed `./main build-type -h`
                if (i > 1 && args[i - 1][0] != '-') {
                    specific = args[i - 1];
                }
                // Check if they typed `./main -h build-type`
                else if (i + 1 < args.size() && args[i + 1][0] != '-') {
                    specific = args[i + 1];
                }
                print_help(prog_name, specific);
                std::exit(0);
            }
        }

        // 2. Load schema defaults
        for (const auto &[flag, opt] : options) {
            data[flag] = opt.default_val;
        }

        // 3. Parse arguments
        for (auto i{1uz}; i < static_cast<std::size_t>(argc); ++i) {
            std::string_view curr = args[i];
            auto eq_idx = curr.find_first_of('=');

            if (eq_idx == std::string_view::npos) {
                // Not an equals flag. Only add it to data if it wasn't swallowed as a specific-help argument.
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

            // Fallback for unregistered flags
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

    template <typename T>
    auto get_val(std::string_view key) const -> std::expected<T, bld::Err>
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

private:
    Config() = default;
};

}; // namespace bld

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
auto main(int argc, char *argv[]) -> int
{
    bld::rebuild_this_when_needed_ext(argc, argv);

    auto &cfg = bld::Config::get();

    cfg.add_option("--debug", bld::Config::Bool, "Enable verbose output", false)
        .add_option("jobs", bld::Config::Int, "Concurrent jobs", 8)
        .add_option("build-type", bld::Config::String, "Build profile", std::string{"rel"}, {"rel", "debug", "profile"})
        .parse(argc, argv);

    if (cfg["--debug"]) {
        bld::log::d("Verbose mode is enabled!");
    }

    if (cfg["build-type"]) {
        std::string mode = cfg["build-type"];
        bld::log::i("Compiling with mode: {}", mode);
    }

    if (cfg["custom_id"]) {
        std::string id = cfg["custom_id"];
        bld::log::i("Found custom ID: {}", id);
    }
    return 0;
}
