# bld

A single-header C++ build system. Your build script is a plain C++ program —
no new tools, languages, or syntax to learn.

## Features

- **No dependencies**: one header (`b_ldr.hpp`), include it and go.
- **Commands & processes**: run programs sync or async, redirect stdin/stdout/stderr
  to files or pipes, capture output into strings.
- **Logging**: leveled logging with colors and custom sinks.
- **Incremental builds**: `bld::is_outdated` and `rebuild_this_when_needed`, so a
  build script recompiles itself when it (or the header) changes.
- **Unified runs**: one command, a parallel task set, an incremental dependency
  plan, or a `compile_commands.json` file, all through the same scheduler.
- **Config parsing**: declare options, get a generated `--help`, read values
  with `cfg["key"]`.
- **Filesystem, strings, time, diff testing** included.
- **Cross-platform**: Linux/macOS (POSIX) and Windows (MSVC, MinGW, clang-cl).

## Installation

Drop `b_ldr.hpp` into your project. In **exactly one** translation unit:

```cpp
#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"
```

Compile with C++23:

```bash
g++ -std=c++23 -I. build.cpp -o build
```

## Usage

### Build script skeleton

Recompile the script itself when its source (or `b_ldr.hpp`) changes:

```cpp
int main(int argc, char *argv[])
{
    if (auto res = bld::rebuild_this_when_needed_ext(argc, argv, {"-I."}); !res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }
    // ... do the actual build ...
}
```

### Logging

```cpp
bld::log::i("Building {} targets", 42);
bld::log::w("This looks suspicious");
bld::log::e("Something failed");
bld::log::d("verbose detail");          // hidden unless min level is lowered
bld::log::set_min_level(bld::Logger::Level::dbg);

// Indent nested sections (RAII scope restores the level):
bld::log::i("building app");
{
    bld::log::indent_scope nest;
    bld::log::i("compiling foo.cpp");
}
```

### Running commands

```cpp
bld::Cmd gcc{"g++", "-c", "foo.cpp", "-o", "foo.o"};

if (auto proc = bld::run(gcc); proc && proc->status_code() == 0) {
    // success
}

// async: the process keeps running; you control when to wait
auto proc = bld::run(bld::Cmd{"sleep", "5"}, bld::async{});
auto status = proc->wait();

// redirect output (no shell involved): one struct per stream, each taking a
// borrowed fd, a lazy path, an eager io_out::open(...) value, or io_out_err{...}
// for merged stdout+stderr
bld::run(gcc, bld::io_out{"build.log"}, bld::io_err{"build.err"});
bld::run(gcc, bld::io_out_err{"build.log"});

// capture into strings (borrowed, must outlive the wait)
std::string out, err, merged;
bld::run(gcc, bld::io_out{&out}, bld::io_err{&err});
bld::run(gcc, bld::io_out_err{&merged});

// feed stdin from a string (fed while waiting, then EOF)
bld::run(bld::Cmd{"cat"}, bld::in_str{"hello"});

// dry_run: log-only preview, spawns nothing
bld::run(gcc, bld::dry_run{});
```

### Capturing output

```cpp
// Merged stdout+stderr, returned as a string on exit 0.
// Non-zero exit becomes an Err carrying the output in Err::output.
auto out = bld::capture(bld::Cmd{"git", "status"});
if (out) { bld::log::i("{}", *out); }

auto merged = bld::capture(bld::Cmd{"clang", "-x", "c++", "-"}, bld::in_str{"int main(){}"});

// Captured output is normalized (\r\n -> \n); pass raw_crlf{} to keep bytes as-is.
// dry_run logs what would run, spawns nothing, returns empty success.
```

### Incremental dependency plan

```cpp
bld::Plan build;
build.add("main.o", bld::Cmd{"g++", "-c", "main.cpp", "-o", "main.o"});
build.needs("main.o", "main.cpp");
build.produces("main.o", "main.o");
build.mark_compile_command("main.o");
build.add("main", bld::Cmd{"g++", "main.o", "-o", "main"});
build.needs("main", "main.o");
build.produces("main", "main");

// The second task runs after main.o because it declares main.o as an input.
auto result = bld::run(build, bld::jobs{8});
```

### Config

```cpp
auto &cfg = bld::Config::get();
cfg.add_option("jobs", bld::Config::Int, "number of parallel jobs", 4)
   .add_option("mode", bld::Config::String, "build mode", std::string{"debug"},
               {"debug", "release"})
   .add_option("verbose", bld::Config::Bool, "verbose output", false);

auto res = cfg.parse(argc, argv);           // never exits the program
if (!res) { bld::log::e("{}", res.error()); return EXIT_FAILURE; }
if (res->help_requested) return EXIT_SUCCESS;   // --help was printed

int    jobs    = int(cfg["jobs"]);
bool   verbose = bool(cfg["verbose"]);
auto   mode    = std::string(cfg["mode"]);
```

Run the script with `./build jobs=8 mode=release verbose`.

### Filesystem

```cpp
bld::fs::write_file("out.txt", "hello");
bld::fs::append_file("out.txt", " world");
auto content = bld::fs::read_file("out.txt");   // expected<std::string>

bld::fs::make_dirs("build/obj", "build/bin");

auto srcs = bld::fs::find_by_ext(".", ".cpp");  // expected<vector<string>>
bld::fs::Controller wctl;
wctl.opts.skip = {"build", ".git"};
std::vector<bld::fs::Dir_entry> found;
for (const auto &e : bld::fs::walk_dir("src", wctl)) {
    if (e.is_file() && (e.extension() == ".cpp" || e.extension() == ".hpp")) {
        found.push_back(e);
    }
}
```

### Unified runs and compile databases

```cpp
std::vector<bld::Task> tasks;
tasks.emplace_back(bld::Cmd{"g++", "-c", "a.cpp", "-o", "a.o"});
tasks.emplace_back(bld::Cmd{"g++", "-c", "b.cpp", "-o", "b.o"});

// jobs{nullopt} => max-1; <=0 => max+i; >0 => capped by max.
// max_async{0} => follow jobs width; >0 => absolute proc cap.
auto res = bld::run(tasks, bld::jobs{4}, bld::max_async{8});

// Export marked tasks, or execute an existing database directly.
bld::run(build, bld::write_compile_commands{"compile_commands.json"});
bld::run(bld::compile_commands("build/compile_commands.json"), bld::jobs{8});

// span<Task> runs all by default; deduce_dependency builds a DAG
// from Task.inputs/outputs/after. Plan always uses its own graph.
bld::Task a{bld::Cmd{"sh", "-c", "echo a > a.o"}}; a.produces("a.o");
bld::Task b{bld::Cmd{"sh", "-c", "cat a.o > b"}}; b.needs("a.o");
std::vector<bld::Task> chain{std::move(a), std::move(b)};
bld::run(chain, bld::deduce_dependency{});
```

Misuse fails fast: duplicates (`label` twice, `jobs` twice) are compile
errors; bad combinations (deps without `deduce_dependency`,
`deduce_dependency` with a `Plan`, empty commands, cycles, self-deps, missing
`cwd`) are runtime `Err`s naming the culprit. `examples/run.cpp` covers each case.

### Diff / testing

```cpp
auto diff = bld::test::compute_diff(old_text, new_text);   // Myers diff
if (!diff.same) {
    bld::log::i("{}", diff);   // colored +/- output
}
```

## Examples

Each file in `examples/` is a complete, runnable script:

```bash
g++ -std=c++23 -I. examples/hello.cpp -o hello
```

| Example | Shows |
| --- | --- |
| `examples/hello.cpp` | Minimal script: rebuild helper, logging, running a command |
| `examples/logging.cpp` | Levels, colors, `set_min_level`, custom streams/logger |
| `examples/commands.cpp` | Sync/async runs, exit codes, file redirection |
| `examples/capture.cpp` | Merged capture, stdin injection, CRLF normalization |
| `examples/files.cpp` | Reading/writing, directories, `walk`, find helpers |
| `examples/walk.cpp` | Callback traversal: prune, stop, fail, error kinds |
| `examples/config.cpp` | Options, types, choices, `--help`, proxy reads |
| `examples/tasks.cpp` | Parallel task batches, failure handling |
| `examples/build_system.cpp` | A small incremental build of several files |
| `examples/run.cpp` | Scheduler behavior and error cases |
| `examples/showcase.cpp` | Self-rebuilding script, options, plans, capture in one file |

## Author

Rinav (GitHub: [rrrinav](https://github.com/rrrinav))
