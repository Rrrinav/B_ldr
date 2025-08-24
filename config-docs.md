# Configuration System

> AI generated

## Overview

The `Config` class is a **singleton configuration manager**.

It allows you to:

* Define **built-in options** (compiler, target, flags, threads, etc.).
* Add **custom options** and **flags** dynamically.
* Parse options from:
  * **Command-line arguments** (`-key=value`, `-flag`).
  * **Configuration file** (`build.conf`).
* Access config values in a **map-like style** (`Config::get()["key"]`).
* Save/load configuration to/from files.
* Display built-in and custom options with `show_help()`.
* Debug/dump all values.

---

## Built-in Options

These are initialized automatically:

| Key/Short            | Description                                                    |
| -------------------- | -------------------------------------------------------------- |
| `-compiler`, `-c`    | Compiler to use (default: detected `clang++`, `g++`, or `cl`). |
| `-target`, `-t`      | Target executable name (default: `main`).                      |
| `-build-dir`, `-d`   | Build directory (default: `./build`).                          |
| `-flags`, `-f`       | Compiler flags (default: `-O2`).                               |
| `-link`, `-l`        | Linker flags (default: empty).                                 |
| `-threads`, `-j`     | Number of build threads (default: `1`).                        |
| `-pre`               | Pre-build command.                                             |
| `-post`              | Post-build command.                                            |
| `-verbose`, `-v`     | Enable verbose output.                                         |
| `-hot-reload`, `-hr` | Enable hot reload and file watching.                           |
| `-override-run`      | Override default run behavior.                                 |
| `-help`, `-h`        | Show help screen.                                              |

---

## Custom Options

You can add your own:

```cpp
auto &cfg = Config::get();

// Add a flag (no value, just true/false)
cfg.add_flag("debug", "Enable debug mode");

// Add an option (key=value style)
cfg.add_option("optimization", "O3", "Optimization level");
```

They automatically appear in the help screen.

---

## Accessing Values

You can access options in two main ways:

### As strings

```cpp
std::string compiler = Config::get()["compiler"]
std::string flags    = Config::get()["flags"];
```

### As booleans

```cpp
if (Config::get()["verbose"]) {
    std::cout << "Verbose mode enabled!\n";
}
```

### As integers

```cpp
int threads = Config::get()["threads"].as_int();
```

### Checking existence

```cpp
if (Config::get()["debug"].exists()) {
    std::cout << "Debug flag is defined!\n";
}
```

### Comparisons

```cpp
if (Config::get()["compiler"] == "clang++") {
    std::cout << "Using Clang\n";
}
```

---

## Parsing Arguments

From `main(int argc, char* argv[])`:

```cpp
handle_args(argc, argv);
```

This will:

* Store all command-line args in `Config::get().cmd_args`.
* Parse `-key=value` pairs and `-flags`.
* Sync recognized built-in flags/values.

Example CLI usage:

```bash
./build -compiler=clang++ -flags="-Wall -Wextra" -threads=4 -verbose -debug
```

---

## Saving and Loading Config Files

### Save to `build.conf`:

```cpp
Config::get().save_to_file();
```

This writes current config values into a human-readable file.

Example file (`build.conf`):

```conf
compiler=clang++
target=main
build-dir=./build
flags=-O2
threads=4
verbose
debug
```

### Load from `build.conf`:

```cpp
Config::get().load_from_file();
```

This will parse and apply stored values.

---

## Commands

The helper functions `handle_args` and `handle_config_command` provide ready-to-use CLI commands.

* `-configure`
  Saves current args into `build.conf`.

  ```bash
  ./build -configure -compiler=clang++ -threads=8
  ```

  → creates/updates `build.conf`.

* `-use-config`
  Loads config from `build.conf`.

  ```bash
  ./build -use-config
  ```

* Normal usage (direct args):

  ```bash
  ./build -compiler=g++ -O2 -verbose
  ```

---

## Help Display

```cpp
Config::get().show_help();
```

Outputs something like:

```
Config Usage:
Flags (no value needed):
  -flag_name              Set flag (e.g., -test, -debug, -verbose)

Key=Value pairs:
  -key=value              Set config value (e.g., -compiler=clang++)

Built-in options:
  -c, -compiler=COMPILER  Compiler to use
  -t, -target=TARGET      Target executable name
  -f, -flags=FLAGS        Compiler flags
  -j, -threads=N          Build threads
  -v, -verbose            Enable verbose output
  -hr, -hot-reload        Enable hot reload
  --watch=files           Comma-separated files to watch

Custom options:
  -debug                  Enable debug mode
  -optimization=VALUE     (default: O3) Optimization level

Any other -key=value and -flags are automatically stored!
```

---

## Debugging

You can dump the full state:

```cpp
Config::get().dump();
```

Output:

```
=== Config Dump ===
Flags:
  verbose
  debug
Values:
  compiler = clang++
  target = main
  build-dir = ./build
  flags = -O2
  threads = 4
  optimization = O3
==================
```

---

**In short:**

* Use `Config::get()["key"]` everywhere in your program.
* Options can come from CLI or config file.
* Extend easily with `add_flag` and `add_option`.
* Save/load configs automatically.
