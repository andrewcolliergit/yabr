#ifndef TASKS_H
#define TASKS_H

#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#include "path.h"

namespace yabr::task {
enum class Conflict {
   none,
   dupe,
   exists_on_disk,
   needs_parent,
   rm_non_empty_dir,
   rm_empty_dir,
   rm_file,
   target_is_child_of_source
};

// The state of "Apply to all" per action type
enum class Action { none, skip, force };
enum class CollisionAction { none, skip, force, increment };
enum class Operation { none, rename, remove, mkparent_and_rename };

struct Task {
   std::filesystem::path source;
   std::filesystem::path target = {};
   Conflict conflict = Conflict::none;
   Operation operation = Operation::none;
   bool is_directory = false;
   bool needs_parent = false;
   bool finished = false;
   size_t in_degree = 0;
   std::vector<size_t> blocks = {};
};

struct ValidationInfo {
   std::unordered_set<std::string> sources_set = {};
   std::unordered_set<std::string> seen_targets = {};
   size_t conflict_num = 0;
   size_t conflicts_detected = 0;
   Action rm_non_empty_dir_action = Action::none;
   Action rm_empty_dir_action = Action::none;
   Action rm_file_action = Action::none;
   Action mkdir_action = Action::none;
   CollisionAction recurring_collision_action = CollisionAction::none;
};

std::vector<Task> create_tasks(std::vector<std::string>& target_list,
                               const std::vector<Source>& sources,
                               std::error_code& ec);

void validate_tasks(std::vector<Task>& tasks);
void execute(const std::vector<Task>& tasks, bool exec = false);
}  // namespace yabr::task

#endif
