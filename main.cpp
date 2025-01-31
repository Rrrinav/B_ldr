#include <iomanip>

#define B_LDR_IMPLEMENTATION
#define BLD_USE_CONFIG
#include <condition_variable>
#include <mutex>
#include <thread>

#include "./b_ldr.hpp"

using namespace bld;
//
//bool Dep_graph::build_parallel(const std::string &target, size_t thread_count)
//{
//  if (thread_count == 0)
//    thread_count = 1;
//  if (thread_count > std::thread::hardware_concurrency())
//    thread_count = std::thread::hardware_concurrency();
//
//  std::unordered_set<std::string> visited, in_progress;
//  if (detect_cycle(target, visited, in_progress))
//  {
//    bld::log(bld::Log_type::ERROR, "Circular dependency detected for target: " + target);
//    return false;
//  }
//
//  std::queue<std::string> ready_targets;
//  if (!prepare_build_graph(target, ready_targets))
//    return false;
//
//  std::mutex queue_mutex, log_mutex;
//  std::condition_variable cv;
//  std::atomic<bool> build_failed{false};
//  std::atomic<size_t> active_builds{0};
//  std::atomic<size_t> completed_targets{0};
//  const size_t total_targets = nodes.size();
//
//  auto worker = [&]()
//  {
//    while (!build_failed)
//    {
//      std::string current_target;
//      {
//        std::unique_lock<std::mutex> lock(queue_mutex);
//        cv.wait(lock, [&]() { return !ready_targets.empty() || (completed_targets == total_targets) || build_failed; });
//
//        if (ready_targets.empty() || build_failed)
//          return;
//
//        current_target = ready_targets.front();
//        ready_targets.pop();
//        nodes[current_target]->in_progress = true;
//        active_builds++;
//      }
//
//      auto node = nodes[current_target].get();
//      if (!node->dep.is_phony && !node->dep.command.is_empty())
//      {
//        {
//          std::lock_guard<std::mutex> log_lock(log_mutex);
//          bld::log(bld::Log_type::INFO, "Building target: " + current_target);
//        }
//
//        if (execute(node->dep.command) <= 0)
//        {
//          {
//            std::lock_guard<std::mutex> log_lock(log_mutex);
//            bld::log(bld::Log_type::ERROR, "Failed to build target: " + current_target);
//          }
//          build_failed = true;
//          cv.notify_all();
//          return;
//        }
//      }
//
//      process_completed_target(current_target, ready_targets, queue_mutex, cv);
//      completed_targets++;
//      active_builds--;
//      cv.notify_all();
//    }
//  };
//
//  std::vector<std::thread> workers;
//  for (size_t i = 0; i < thread_count; ++i)
//    workers.emplace_back(worker);
//
//  for (auto &t : workers)
//    if (t.joinable())
//      t.join();
//
//  return !build_failed;
//}
//
//bool Dep_graph::prepare_build_graph(const std::string &target, std::queue<std::string> &ready_targets)
//{
//  auto it = nodes.find(target);
//  if (it == nodes.end())
//  {
//    if (std::filesystem::exists(target))
//    {
//      if (checked_sources.find(target) == checked_sources.end())
//      {
//        bld::log(bld::Log_type::INFO, "Using existing source file: " + target);
//        checked_sources.insert(target);
//      }
//      return true;
//    }
//    bld::log(bld::Log_type::ERROR, "Target not found: " + target);
//    return false;
//  }
//
//  auto node = it->second.get();
//  if (node->visited)
//    return true;
//  node->visited = true;
//
//  // Recursively prepare dependencies
//  for (const auto &dep : node->dependencies)
//  {
//    if (!prepare_build_graph(dep, ready_targets))
//      return false;
//    node->waiting_on.push_back(dep);
//  }
//
//  // If no dependencies exists **OR** all dependencies are ready, add to ready queue
//  if (node->waiting_on.empty())
//    ready_targets.push(target);
//
//  return true;
//}
//
//void Dep_graph::process_completed_target(const std::string &target, std::queue<std::string> &ready_targets, std::mutex &queue_mutex,
//                                              std::condition_variable &cv)
//{
//  std::lock_guard<std::mutex> lock(queue_mutex);
//  nodes[target]->checked = true;
//  nodes[target]->in_progress = false;
//
//  // Find all nodes that were waiting on this target
//  for (const auto &node_pair : nodes)
//  {
//    auto &node = node_pair.second;
//    if (!node->checked && !node->in_progress)
//    {
//      auto &waiting = node->waiting_on;
//      waiting.erase(std::remove(waiting.begin(), waiting.end(), target), waiting.end());
//
//      // If no more dependencies, add to ready queue
//      if (waiting.empty())
//        ready_targets.push(node_pair.first);
//    }
//  }
//}
//
//bool Dep_graph::build_all_parallel(size_t thread_count)
//{
//  std::vector<std::string> root_targets;
//  for (const auto &node : nodes)
//  {
//    bool is_dependency = false;
//    for (const auto &other : nodes)
//    {
//      if (std::find(other.second->dep.dependencies.begin(), other.second->dep.dependencies.end(), node.first) !=
//          other.second->dep.dependencies.end())
//      {
//        is_dependency = true;
//        break;
//      }
//    }
//    if (!is_dependency)
//      root_targets.push_back(node.first);
//  }
//
//  // Create a master phony target that depends on all root targets
//  add_phony("__master_target__", root_targets);
//  bool result = build_parallel("__master_target__", thread_count);
//
//  // Clean up the master target
//  nodes.erase("__master_target__");
//  return result;
//}

int main(int argc, char *argv[])
{
  Dep_graph graph;

}
