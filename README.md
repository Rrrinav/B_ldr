# Builder (bld)

A single-header C++ build system. Your build script is a plain C++ program — no new tools,
no new languages, no new syntax to learn.

Because [Tsoding](https://github.com/tsoding) said you should write your own build system.

## Features

- **No dependencies**: one header (`b_ldr.hpp`), include it and go.
- **Commands & processes**: run programs synchronously or asynchronously, route stdin/stdout/stderr
  to files or pipes, capture output into strings.
- **Logging**: leveled logging (`DEBUG`/`INFO`/`WARN`/`ERROR`/`FATAL`) with colors, custom sinks.
- **Incremental builds**: `bld::is_outdated` and `rebuild_this_when_needed` so your build script
  recompiles itself when it (or the header) changes.
- **Parallel tasks**: run batches of commands concurrently, wait for all, stop on first failure.
- **Config parsing**: declare options, get a generated `--help`, read values with `cfg["key"]`.
- **Filesystem + strings + time + diff testing**: batteries included.
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

Every build script should start by recompiling itself when the script (or `b_ldr.hpp`) changes:

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
bld::log::i(std::cerr, "to a specific stream");
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

// redirect output to a file (no shell involved)
bld::run(gcc, bld::out_f{"build.log"}, bld::err_f{"build.err"});
```

### Capturing output

```cpp
std::string out, err;
auto status = bld::capture(bld::Cmd{"git", "status"}, bld::cap_out{out}, bld::cap_err{err});

std::string merged;
bld::capture(bld::Cmd{"clang", "-x", "c++", "-"}, bld::cap_merge{merged}, bld::in_str{"int main(){}"});

// Captured output is always normalized: \r\n -> \n (so Windows text matches "\n"-terminated strings)
bld::capture(bld::Cmd{"dir"}, bld::cap_merge{merged});

// ...except when you pass raw_crlf{} to keep the bytes as-is:
bld::capture(bld::Cmd{"dir"}, bld::cap_merge{merged}, bld::raw_crlf{});
```

### Incremental dependency graph

```cpp
if (bld::is_outdated("main", std::array{"main.cpp", "foo.cpp"}) || bld::is_outdated("main", "main.cpp")) {
    bld::run(bld::Cmd{"g++", "main.cpp", "-o", "main"});
}
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
auto   mode    = std::string(cfg["mode"]);  // throws on missing/mismatched key
```

Run the script with `./build jobs=8 mode=release verbose`.

### Filesystem

```cpp
bld::fs::write_file("out.txt", "hello");
bld::fs::append_file("out.txt", " world");
auto content = bld::fs::read_file("out.txt");   // expected<std::string>

bld::fs::make_dirs("build/obj", "build/bin");

auto srcs = bld::fs::find_by_ext(".", ".cpp");  // expected<vector<string>>
auto walk = bld::fs::Dir_walker{"src"}
    .ext({".cpp", ".hpp"})
    .skip({"build", ".git"})
    .collect();                                 // expected<vector<Dir_entry>>
```

### Parallel tasks

```cpp
std::vector<bld::Task> tasks;
tasks.emplace_back(bld::Cmd{"g++", "-c", "a.cpp", "-o", "a.o"});
tasks.emplace_back(bld::Cmd{"g++", "-c", "b.cpp", "-o", "b.o"});

if (auto res = bld::run(tasks, /*max_jobs=*/4); !res) {
    bld::log::e("build stopped: {}", res.error());
}

// Same tasks, but scheduled on a real worker-thread pool instead of batches:
if (auto res = bld::run_threaded(tasks, /*threads=*/4); !res) {
    bld::log::e("build stopped: {}", res.error());
}
```

### Diff / testing

```cpp
auto diff = bld::test::compute_diff(old_text, new_text);   // Myers diff
if (!diff.same) {
    bld::log::i("{}", diff);   // pretty, colored +/- output
}
```

## Examples

Each file in `examples/` is a complete, runnable script. Build any of them with:

```bash
g++ -std=c++23 -I. examples/hello.cpp -o hello
```

| Example | Shows |
| --- | --- |
| `examples/hello.cpp` | Minimal script: rebuild helper, logging, running a command |
| `examples/logging.cpp` | Levels, colors, `set_min_level`, custom streams/logger |
| `examples/commands.cpp` | Sync/async runs, exit codes, file redirection |
| `examples/capture.cpp` | `cap_out`/`cap_err`/`cap_merge`, stdin injection, default CRLF normalization + `raw_crlf{}` opt-out |
| `examples/files.cpp` | Reading/writing, directories, `Dir_walker`, find helpers |
| `examples/config.cpp` | Options, types, choices, `--help`, proxy reads |
| `examples/tasks.cpp` | Parallel task batches, failure handling |
| `examples/build_system.cpp` | A small real incremental build of several files |

## TODO

- [X] Parallel incremental builds
- [ ] Fully cross-platform functions (progress: header + tests compile under MSVC/MinGW)

## Author

Rinav (GitHub: [rrrinav](https://github.com/rrrinav))
