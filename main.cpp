#include <initializer_list>
#include <unordered_set>
#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include "./b_ldr.hpp"
#define BLD_USE_CONFIG

struct Dep
{
  std::string target;                     // Target/output file
  std::vector<std::string> dependencies;  // Input files/dependencies
  bld::Command command;                   // Command to build the target
  bool is_phony{false};                    // Whether this is a phony target

  // Default constructor
  Dep() = default;

  // Constructor for non-phony targets
  Dep(std::string target, std::vector<std::string> dependencies = {}, bld::Command command = {})
      : target(std::move(target)), dependencies(std::move(dependencies)), command(std::move(command)) {}

  // Constructor for phony targets
  Dep(std::string target, std::vector<std::string> dependencies, bool is_phony)
      : target(std::move(target)), dependencies(std::move(dependencies)), is_phony(is_phony) {}

  // Implicit conversion from initializer list for better usability
  Dep(std::string target, std::initializer_list<std::string> deps, bld::Command command = {})
      : target(std::move(target)), dependencies(deps), command(std::move(command)) {}

  // Copy constructor
  Dep(const Dep& other)
      : target(other.target), dependencies(other.dependencies), command(other.command), is_phony(other.is_phony) {}

  // Move constructor
  Dep(Dep&& other) noexcept
      : target(std::move(other.target)),
        dependencies(std::move(other.dependencies)),
        command(std::move(other.command)),
        is_phony(other.is_phony) {}

  // Copy assignment
  Dep& operator=(const Dep& other)
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
  Dep& operator=(Dep&& other) noexcept
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
};

class Dep_graph
{
  struct Node
  {
    Dep dep;
    std::vector<std::string> dependencies;  // Names of dependent nodes
    bool visited{false};
    bool in_progress{false};                // For cycle detection
    bool checked{false};                    // For caching the state

    Node(const Dep &d) : dep(d) {}
  };
  std::unordered_map<std::string, std::unique_ptr<Node>> nodes;
  std::unordered_set<std::string> checked_sources;  // Track which source files we've already checked

public:
  void add_dep(const Dep &dep)
  {
    // We create a node and save it to map with key "File"
    auto node = std::make_unique<Node>(dep);
    node->dependencies = dep.dependencies;
    nodes[dep.target] = std::move(node);
  }

  void add_phony(const std::string &target, const std::vector<std::string> &deps)
  {
    Dep phony_dep;
    phony_dep.target = target;
    phony_dep.dependencies = deps;
    phony_dep.is_phony = true;
    add_dep(phony_dep);
  }

  bool needs_rebuild(const Node *node)
  {
    if (node->dep.is_phony)
      return true;

    auto target_time = std::filesystem::file_time_type::min();
    if (std::filesystem::exists(node->dep.target))
      return true;

    target_time = std::filesystem::last_write_time(node->dep.target);

    for (const auto &dep : node->dep.dependencies)
    {
      if (!std::filesystem::exists(dep))
      {
        bld::log(bld::Log_type::ERROR, "Dependency does not exist: " + dep);
        return false;  // Returning false instead of true to avoid rebuilding on missing dependencies.
      }

      if (std::filesystem::last_write_time(dep) > target_time)
        return true;
    }

    // Only return true if the target does not exist *AND* at least one dependency exists
    return !std::filesystem::exists(node->dep.target) && !node->dep.dependencies.empty();
  }

  bool build(const std::string &target)
  {
    std::unordered_set<std::string> visited, in_progress;
    if (detect_cycle(target, visited, in_progress))
    {
      bld::log(bld::Log_type::ERROR, "Circular dependency detected for target: " + target);
      return false;
    }
    return build_node(target);
  }

  bool build(const Dep &dep)
  {
    add_dep(dep);
    return build(dep.target);
  }

  bool build_all()
  {
    bool success = true;
    for (const auto &node : nodes)
      if (!build(node.first))
        success = false;
    return success;
  }

private:

  bool build_node(const std::string &target)
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
    if (node->checked)
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

  bool detect_cycle(const std::string &target, std::unordered_set<std::string> &visited, std::unordered_set<std::string> &in_progress)
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
};

int main(int argc, char *argv[])
{
  // Check if the executable needs to be rebuilt and restart if necessary
  BLD_REBUILD_YOURSELF_ONCHANGE();

  // Handle command-line arguments
  BLD_HANDLE_ARGS();

}
