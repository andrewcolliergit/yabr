#include "sort.h"

#ifdef _WIN32
#include <process.h>
#define GETPID _getpid
#else
#include <unistd.h>
#define GETPID getpid
#endif

#include <algorithm>
#include <queue>
#include <string>
#include <unordered_map>

using namespace std;
namespace fs = std::filesystem;

namespace {
bool is_descendant(const fs::path& descendant, const fs::path& ancestor) {
   auto it_d = descendant.begin();
   auto it_a = ancestor.begin();
   while (it_a != ancestor.end()) {
      if (it_d == descendant.end() || *it_d != *it_a) return false;
      ++it_d;
      ++it_a;
   }
   return it_d !=
          descendant.end();  // must have more components to be a descendant
}
}  // namespace

namespace yabr::task::sort {

void depth_first(vector<Task>& tasks) {
   // Path depth where root = 0
   auto depth = [](const fs::path& p) -> size_t {
      return std::distance(p.begin(), p.end()) - 1;
   };

   // Sort tasks by path depth from deepest to shallowest
   std::sort(tasks.begin(), tasks.end(), [&](const Task& a, const Task& b) {
      return depth(a.source) > depth(b.source);
   });
}

vector<Task> topo_graph(vector<Task> tasks) {
   const string temp_suffix = ".yabr-" + std::to_string(GETPID()) + ".tmp";
   std::vector<Task> final;
   unordered_map<fs::path, size_t> sources;

   // Map source paths to their index in the pool for O(1) lookup
   for (size_t i = 0; i < tasks.size(); ++i) {
      sources[tasks[i].source] = i;
   }

   // Link targets to the tasks they block
   for (size_t i = 0; i < tasks.size(); ++i) {
      // If the target of task 'i' is the source of task 'j'
      if (sources.contains(tasks[i].target)) {
         size_t occupant_idx = sources[tasks[i].target];

         // Mover (i) must wait for Occupant (occupant_idx)
         tasks[i].in_degree = 1;

         // Occupant (occupant_idx) blocks Mover (i)
         tasks[occupant_idx].blocks.push_back(i);
      }

      // New: target of i is inside source of j
      for (size_t j = 0; j < tasks.size(); ++j) {
         if (i == j) continue;
         if (is_descendant(tasks[i].target, tasks[j].source)) {
            // i must execute before j
            tasks[j].in_degree = 1;
            tasks[i].blocks.push_back(j);
         }
      }
   }

   // Populate a queue with tasks that are ready to execute
   std::queue<size_t> ready_queue;
   for (size_t i = 0; i < tasks.size(); ++i) {
      if (tasks[i].in_degree == 0) ready_queue.push(i);
   }

   while (final.size() < tasks.size()) {
      // Standard moves
      while (!ready_queue.empty()) {
         size_t idx = ready_queue.front();
         ready_queue.pop();

         final.push_back(tasks[idx]);
         tasks[idx].finished = true;

         // Notify the mover waiting for this path
         if (!tasks[idx].blocks.empty()) {
            for (size_t blocked_idx : tasks[idx].blocks) {
               tasks[blocked_idx].in_degree--;
               if (tasks[blocked_idx].in_degree == 0) {
                  ready_queue.push(blocked_idx);
               }
            }
            tasks[idx].blocks.clear();
         }
      }

      // Cycle Breaking
      if (final.size() < tasks.size()) {
         // Find the first task that hasn't finished (stuck in cycle)
         for (size_t i = 0; i < tasks.size(); ++i) {
            if (!tasks[i].finished) {
               Task& task = tasks[i];

               // Create a dummy move in the execution list
               // Set the target to task.target.tmp
               Task break_task = task;
               break_task.target =
                   task.target.parent_path() /
                   (task.target.filename().string() + temp_suffix);
               final.push_back(break_task);

               // Update the original task to move from tmp to the final
               // target later
               task.source = task.target.parent_path() /
                             (task.target.filename().string() + temp_suffix);
               task.in_degree = 0;
               task.finished = true;

               if (!task.blocks.empty()) {
                  for (size_t blocked_idx : task.blocks) {
                     tasks[blocked_idx].in_degree--;
                     if (tasks[blocked_idx].in_degree == 0) {
                        ready_queue.push(blocked_idx);
                     }
                  }
                  task.blocks.clear();
               }

               ready_queue.push(i);

               break;
            }
         }
      }
   }

   return final;
}

int natural_compare(const std::string& a, const std::string& b) {
   size_t i = 0, j = 0;
   while (i < a.size() && j < b.size()) {
      if (std::isdigit(a[i]) && std::isdigit(b[j])) {
         // Compare numeric chunks as integers
         size_t num_start_a = i, num_start_b = j;
         while (i < a.size() && std::isdigit(a[i])) i++;
         while (j < b.size() && std::isdigit(b[j])) j++;
         int num_a = std::stoi(a.substr(num_start_a, i - num_start_a));
         int num_b = std::stoi(b.substr(num_start_b, j - num_start_b));
         if (num_a != num_b) return num_a - num_b;
      }
      else {
         if (std::tolower(a[i]) != std::tolower(b[j]))
            return std::tolower(a[i]) - std::tolower(b[j]);
         i++;
         j++;
      }
   }
   return static_cast<int>(a.size()) - static_cast<int>(b.size());
}
}  // namespace yabr::task::sort
