#include "tasks.h"

#include <exception>
#include <functional>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "config.h"
#include "ui.h"

namespace fs = std::filesystem;
using std::string, std::vector, std::optional, std::error_code,
    std::unordered_set, std::unordered_map;

namespace {
using namespace yabr::task;

string get_increment(int n, size_t group_size) {
   string str_n = std::to_string(n);
   // Sanity check
   if (group_size == 0) {
      throw std::runtime_error(
          "Error in get_increment_suffix(): group_size is 0");
   }

   size_t width;

   switch (yabr::opt::incfmt->min_digits) {
      case 0:  // Normal
      case 1:
         width = 0;
         break;
      case -1:
         width = static_cast<size_t>(log10(group_size) + 1);
         break;
      default:
         width = static_cast<size_t>(yabr::opt::incfmt->min_digits);
   }

   while (str_n.length() < width) {
      str_n.insert(0, "0");
   }

   return str_n;
}

fs::path number(const fs::path& p, int n, size_t max) {
   if (p.empty()) return p;

   return p.parent_path() / (p.stem().string() + yabr::opt::incfmt->prefix +
                             get_increment(n, max) + yabr::opt::incfmt->suffix +
                             p.extension().string());
}

void increment_targets(std::vector<Task>& tasks) {
   // Collect the number of collisions of each filename
   unordered_map<string, size_t> countdown_map, group_max_map;
   unordered_set<string> set_of_sources;

   for (const Task& task : tasks) {
      set_of_sources.insert(task.source);  // Available sources
      if (task.target.empty()) continue;   // Deletion
      countdown_map[task.target.string()]++;
   }

   group_max_map = countdown_map;
   size_t group_size = 0;
   int count = 0;
   string original_key = "";

   auto available = [&](const fs::path& t, int n) -> bool {
      fs::path potential = number(t, n, group_size);
      return set_of_sources.contains(potential) || !fs::exists(potential);
   };

   // Increment each filename according to the config
   // options and the number of collisions
   for (Task& task : tasks) {
      if (task.target.empty()) continue;  // Deletion
      count = countdown_map[task.target.string()];
      group_size = group_max_map[task.target.string()];
      original_key = task.target.string();

      if (group_size > 1) {
         int n = static_cast<int>(group_size) - static_cast<int>(count) + 1;
         while (!available(task.target, n)) {
            n++;
         }
         task.target = number(task.target, n, group_size);
         countdown_map[original_key] = group_size - n;
      }
   }
}

void increment_single(Task& task,
                      std::unordered_set<string>& seen_targets,
                      const std::unordered_set<string>& sources_set) {
   int n = 1;
   fs::path numbered;
   do {
      numbered = number(task.target, n, 1);
      n++;
   } while (seen_targets.contains(numbered.string()) ||
            (fs::exists(numbered) && !sources_set.contains(numbered.string())));

   task.target = numbered;
   seen_targets.insert(numbered.string());
}

bool target_is_in_source(const Task& task) {
   if (!task.is_directory) return false;
   fs::path rel = task.target.lexically_relative(task.source);
   return (!rel.empty() && !rel.string().starts_with(".."));
}

namespace execute {
void remove(const Task& task, bool exec, std::error_code& ec) {
   // Dry run / preview
   if (!exec) {
      yabr::log::log(
          (yabr::opt::preview) ? yabr::log::Level::preview
                               : yabr::log::Level::dryrun,
          {.action = "Delete",
           .src = yabr::ui::path_str(task.source, task.is_directory)});
      return;
   }

   // Execute removal
   fs::remove_all(task.source, ec);
   if (ec) {
      // Log failure
      yabr::log::log(((yabr::opt::strict) ? yabr::log::Level::error
                                          : yabr::log::Level::warning),
                     {.action = "Failed to delete",
                      .src = yabr::ui::path_str(task.source, task.is_directory),
                      .reason = ec.message()});
      if (yabr::opt::strict) throw std::exception();
   }
   else {
      // Log success
      yabr::log::log(
          yabr::log::Level::success,
          {.action = "Deleted",
           .src = yabr::ui::path_str(task.source, task.is_directory)});
   }
}

void rename(const Task& task, bool exec, std::error_code& ec) {
   auto log_failed_rename = [&]() {
      yabr::log::Level level = (yabr::opt::strict) ? yabr::log::Level::error
                                                   : yabr::log::Level::warning;
      yabr::log::log(level,
                     {.action = "Failed to rename",
                      .src = yabr::ui::path_str(task.source, task.is_directory),
                      .tgt = yabr::ui::path_str(task.target, task.is_directory),
                      .reason = ec.message()});
      if (yabr::opt::strict) throw std::exception();
   };

   auto log_successful_rename = [&]() {
      yabr::log::Level level;
      if (exec) {
         level = yabr::log::Level::success;
      }
      else if (yabr::opt::preview) {
         level = yabr::log::Level::preview;
      }
      else if (yabr::opt::dry_run) {
         level = yabr::log::Level::dryrun;
      }
      else {
         level = yabr::log::Level::success;
      }
      yabr::log::log(
          level, {.src = yabr::ui::path_str(task.source, task.is_directory),
                  .tgt = yabr::ui::path_str(task.target, task.is_directory)});
   };

   if (!exec) {
      log_successful_rename();
      return;
   }

   if (!ec) {
      fs::rename(task.source, task.target, ec);
      if (ec) {
         log_failed_rename();
      }
      else {
         log_successful_rename();
      }
   }
}

void make_parent_path(const Task& task, bool exec, std::error_code& ec) {
   auto log_successful_mkdir = [&]() {
      yabr::log::Level level;
      if (exec) {
         level = yabr::log::Level::success;
      }
      else if (yabr::opt::preview) {
         level = yabr::log::Level::preview;
      }
      else if (yabr::opt::dry_run) {
         level = yabr::log::Level::dryrun;
      }
      else {
         level = yabr::log::Level::success;
      }
      const char* action = (exec) ? "Created path" : "Create path";
      yabr::log::log(
          level, {.action = action, .src = task.target.parent_path().string()});
   };
   auto log_failed_mkdir = [&]() {
      yabr::log::Level level = (yabr::opt::strict) ? yabr::log::Level::error
                                                   : yabr::log::Level::warning;
      yabr::log::log(level, {.action = "Failed to create path",
                             .src = task.target.parent_path().string(),
                             .reason = ec.message()});
      if (yabr::opt::strict) throw std::exception();
   };

   // Dry run
   if (!exec) {
      log_successful_mkdir();
      return;
   }

   fs::create_directories(task.target.parent_path(), ec);
   if (ec) {
      log_failed_mkdir();
   }
   else {
      log_successful_mkdir();
   }
}
}  // namespace execute

namespace resolution {
void remove_target_if_child_of_own_source(vector<Task>& tasks) {
   tasks.erase(
       std::remove_if(
           tasks.begin(), tasks.end(),
           [](const yabr::task::Task& task) {
              bool remove = target_is_in_source(task);
              if (remove) {
                 yabr::log::log(
                     yabr::log::Level::warning,
                     {.action = "Skipping",
                      .src = yabr::ui::path_str(task.source, task.is_directory),
                      .tgt = yabr::ui::path_str(task.target, task.is_directory),
                      .reason = "Target is a child of its own source"});
              }
              return remove;
           }),
       tasks.end());
}

void detect_problems(vector<Task>& tasks, ValidationInfo& vi) {
   for (const Task& t : tasks) vi.sources_set.insert(t.source.string());

   for (Task& task : tasks) {
      // Set the default conflict to none
      task.conflict = Conflict::none;
      // Deletion:
      if (task.target.empty()) {
         // Directories
         if (task.is_directory) {
            std::error_code ec;
            // Non-empty directory deletin
            if (!fs::is_empty(task.source, ec) && !ec) {
               if (yabr::opt::del && yabr::opt::force) {
                  task.conflict = Conflict::none;
               }
               else {
                  task.conflict = Conflict::rm_non_empty_dir;
                  ++vi.conflicts_detected;
               }
            }
            // Empty directory deletion
            else {
               if (yabr::opt::del) {
                  task.conflict = Conflict::none;
               }
               else {
                  task.conflict = Conflict::rm_empty_dir;
                  ++vi.conflicts_detected;
               }
            }
         }
         // Regular file deletion
         else {
            if (yabr::opt::del) {
               task.conflict = Conflict::none;
            }
            else {
               task.conflict = Conflict::rm_file;
               ++vi.conflicts_detected;
            }
         }
         continue;
      }

      // Detect duplicates and disc collisions.
      // First occurrence of a target is clean;
      // only later ones are dupes
      bool dupe = vi.seen_targets.contains(task.target.string());
      bool exists_on_disk = fs::exists(task.target) &&
                            !vi.sources_set.contains(task.target.string());

      // Duplicate target
      if (dupe) {
         task.conflict = Conflict::dupe;
         ++vi.conflicts_detected;
      }
      // Disc Collision
      else if (exists_on_disk) {
         task.conflict = Conflict::exists_on_disk;
         ++vi.conflicts_detected;
      }
      // Needs parent
      else if (task.needs_parent && !yabr::opt::create_directories) {
         task.conflict = Conflict::needs_parent;
         ++vi.conflicts_detected;
      }
      vi.seen_targets.insert(task.target.string());
   }
}

void handle_issue(vector<Task>& tasks,
                  vector<Task>::iterator& it,
                  Action& recurring_action,
                  bool opt_override,
                  std::function<yabr::ui::prompt::YNAction()> prompt_fn) {
   Action action = Action::none;

   if (opt_override) {
      recurring_action = Action::force;
   }

   if (recurring_action != Action::none) {
      action = recurring_action;
   }
   else if (recurring_action == Action::none) {
      switch (prompt_fn()) {
         case yabr::ui::prompt::YNAction::yesall:
            recurring_action = Action::force;
            [[fallthrough]];
         case yabr::ui::prompt::YNAction::yes:
            action = Action::force;
            break;
         case yabr::ui::prompt::YNAction::noall:
            recurring_action = Action::skip;
            [[fallthrough]];
         case yabr::ui::prompt::YNAction::no:
            action = Action::skip;
            break;
         case yabr::ui::prompt::YNAction::none:
         default:
            throw std::runtime_error(
                "Unexpected YN/all enum encountered: \"none\" in "
                "validate_tasks.");
      }
   }
   switch (action) {
      case Action::force:
         ++it;
         break;
      case Action::skip:
         it = tasks.erase(it);
         break;
      case Action::none:
      default:
         throw std::runtime_error(
             "Unexpected action encountered in validate_tasks.");
   }
}

void collision(vector<Task>& tasks,
               vector<Task>::iterator& it,
               ValidationInfo& vi) {
   // Rename conflict (dupe or exists_on_disk)
   CollisionAction this_collision_action = CollisionAction::none;
   yabr::ui::prompt::CollisionResponse response;  // This is the prompt response
   // If user already specified they want to...
   // Skip all collisions
   if (yabr::opt::skip ||
       vi.recurring_collision_action == CollisionAction::skip) {
      this_collision_action = CollisionAction::skip;
   }
   // Overwrite all collisions
   else if (yabr::opt::force ||
            vi.recurring_collision_action == CollisionAction::force) {
      this_collision_action = CollisionAction::force;
   }
   // Increment all collisions
   else if (vi.recurring_collision_action == CollisionAction::increment) {
      this_collision_action = CollisionAction::increment;
   }
   // ...Otherwise, ask the user what they want to do
   else {
      response = yabr::ui::prompt::collision(
          yabr::ui::path_str(it->source, it->is_directory),
          yabr::ui::path_str(it->target, it->is_directory), vi.conflict_num + 1,
          vi.conflicts_detected);
      switch (response) {
         case yabr::ui::prompt::CollisionResponse::overwrite_all:
            vi.recurring_collision_action = CollisionAction::force;
            [[fallthrough]];
         case yabr::ui::prompt::CollisionResponse::overwrite:
            this_collision_action = CollisionAction::force;
            break;
         case yabr::ui::prompt::CollisionResponse::skip_all:
            vi.recurring_collision_action = CollisionAction::skip;
            [[fallthrough]];
         case yabr::ui::prompt::CollisionResponse::skip:
            this_collision_action = CollisionAction::skip;
            // it = tasks.erase(it);
            break;
         case yabr::ui::prompt::CollisionResponse::increment_all:
            vi.recurring_collision_action = CollisionAction::increment;
            [[fallthrough]];
         case yabr::ui::prompt::CollisionResponse::increment:
            this_collision_action = CollisionAction::increment;
            break;
         default:
            throw std::runtime_error("Unknown error");
      }
   }
   switch (this_collision_action) {
      case CollisionAction::skip:
         it = tasks.erase(it);
         break;
      case CollisionAction::force:
         vi.seen_targets.insert(it->target.string());
         ++it;
         break;
      case CollisionAction::increment: {
         fs::path original_src = it->source;
         increment_single(*it, vi.seen_targets, vi.sources_set);
         if (it->source == it->target) {
            it = tasks.erase(it);
            yabr::log::log(
                yabr::log::Level::info,
                {.action = "Skipping",
                 .src = yabr::ui::path_str(original_src, it->is_directory),
                 .reason = "Incrementing resulted in original name"});
         }
         else {
            ++it;
         }
         break;
      }
      case CollisionAction::none:
         break;
      default:
         throw std::runtime_error(
             "Unexpected enum for this_collision_action in validate_tasks");
   }
}
}  // namespace resolution
}  // namespace

namespace yabr::task {
using yabr::log::log;

vector<Task> create_tasks(vector<string>& target_list,
                          const vector<Source>& sources,
                          error_code& ec) {
   vector<Task> tasks;
   tasks.reserve(sources.size());
   // Collect targets from target_list, which was read from file
   for (size_t i = 0; i < sources.size(); ++i) {
      ec.clear();
      optional<Target> target = Target::create(target_list[i], ec);

      // Log excluded target
      if (ec) {
         log(((opt::strict) ? log::Level::error : log::Level::warning),
             {.action = "Skipping target",
              .src = target_list[i],
              .reason = ec.message()});
         if (opt::strict) throw std::exception();
         continue;
      }

      // Skip unchanged
      if (sources[i].path() == target->path()) continue;

      // Delete
      if (target->requests_deletion) {
         tasks.emplace_back(Task{.source = sources[i].path(),
                                 .operation = Operation::remove,
                                 .is_directory = sources[i].is_directory});
      }
      // Rename
      else {
         tasks.emplace_back(Task{.source = sources[i].path(),
                                 .target = target->path(),
                                 .operation = Operation::rename,
                                 .is_directory = sources[i].is_directory,
                                 .needs_parent = target->needs_parent});
      }
   }
   return tasks;
}

void validate_tasks(vector<Task>& tasks) {
   ValidationInfo vi;

   // Check if target is a child of its own source. There is no way to force
   // this, so we skip // TODO: Let's try to implement this!
   // Use Conflict::target_is_child_of_source
   resolution::remove_target_if_child_of_own_source(tasks);

   // Optionally increment targets (batch mode — no conflict prompting)
   if (opt::increment || opt::incfmt) {
      increment_targets(tasks);

      // Remove any targets that were incremented to their original filename
      tasks.erase(
          std::remove_if(tasks.begin(), tasks.end(),
                         [](const Task& t) { return t.source == t.target; }),
          tasks.end());
   }

   // --- Phase 1: Detect and count conflicts ---
   resolution::detect_problems(tasks, vi);

   // --- Phase 2: Handle conflicts ---
   // Seed seen_targets with clean rename targets so increment_single can
   // avoid them when searching for a free slot
   vi.seen_targets.clear();
   for (const Task& t : tasks) {
      if (!t.target.empty() && t.conflict == Conflict::none) {
         vi.seen_targets.insert(t.target.string());
      }
   }

   // Loop through tasks and handle any conflicts they have
   for (auto it = tasks.begin(); it != tasks.end();) {
      switch (it->conflict) {
         case Conflict::none:
            it->operation = Operation::rename;
            ++it;
            break;
         case Conflict::needs_parent:
            ++vi.conflict_num;
            it->operation = Operation::mkparent_and_rename;
            resolution::handle_issue(
                tasks, it, vi.mkdir_action, opt::create_directories, [&]() {
                   return yabr::ui::prompt::mkparent(
                       yabr::ui::path_str(it->target, it->is_directory),
                       it->target.parent_path().string(), vi.conflict_num,
                       vi.conflicts_detected);
                });
            break;
         case Conflict::rm_non_empty_dir:
            ++vi.conflict_num;
            it->operation = Operation::remove;
            resolution::handle_issue(
                tasks, it, vi.rm_non_empty_dir_action, (opt::del && opt::force),
                [&]() {
                   return yabr::ui::prompt::rm_full(
                       yabr::ui::path_str(it->source, it->is_directory),
                       vi.conflict_num, vi.conflicts_detected);
                });
            // resolution::rm_empty_dir(tasks, it, vi);
            break;
         case Conflict::rm_empty_dir:
            ++vi.conflict_num;
            it->operation = Operation::remove;
            resolution::handle_issue(
                tasks, it, vi.rm_empty_dir_action, opt::del, [&]() {
                   return yabr::ui::prompt::rm_dir(
                       yabr::ui::path_str(it->source, it->is_directory),
                       vi.conflict_num, vi.conflicts_detected);
                });
            // resolution::non_empty_dir(tasks, it, vi);
            break;
         case Conflict::rm_file:
            ++vi.conflict_num;
            it->operation = Operation::remove;
            resolution::handle_issue(
                tasks, it, vi.rm_file_action, opt::del, [&]() {
                   return yabr::ui::prompt::rm_file(
                       yabr::ui::path_str(it->source, it->is_directory),
                       vi.conflict_num, vi.conflicts_detected);
                });
            // resolution::rm_file(tasks, it, vi);
            break;
         case Conflict::dupe:
         case Conflict::exists_on_disk:
            ++vi.conflict_num;
            it->operation = Operation::rename;
            resolution::collision(tasks, it, vi);
            break;
         default:
            throw std::runtime_error("Unexpected conflict in validate_tasks");
      }
   }

   // Print a message that conflicts were resolved
   if (vi.conflict_num > 0) {
      if (tasks.empty()) {
         std::cerr << "All tasks were skipped.\n";
      }
      else {
         std::cerr << "All issues resolved.";
         if (!yabr::opt::exec && !yabr::opt::dry_run &&
             !yabr::opt::input_file) {
            std::cerr << " Previewing tasks...\n";
         }
         else {
            std::cerr << "\n";
         }
      }
   }
}

void execute(const vector<Task>& tasks, bool exec) {
   std::error_code ec;
   size_t failed = 0;
   size_t succeeded = 0;

   if (opt::verbose > 2 && exec) {
      std::cout << "\nExecuting...\n";
   }

   // Execute tasks
   for (const yabr::task::Task& task : tasks) {
      ec.clear();
      switch (task.operation) {
         case Operation::remove:
            execute::remove(task, exec, ec);
            break;
         case Operation::mkparent_and_rename:
            execute::make_parent_path(task, exec, ec);
            if (ec) break;
            [[fallthrough]];
         case Operation::rename:
            execute::rename(task, exec, ec);
            break;
         case Operation::none:
         default:
            throw std::runtime_error(
                "Unexpected operation encountered in execute phase.");
      }

      if (ec) {
         ++failed;
      }
      else {
         ++succeeded;
      }
   }

   // Report results
   if (exec) {
      if (opt::verbose > 2) std::cout << "\n";
      if (opt::verbose > 2 && succeeded > 0) {
         std::cout << ui::color::green("Successfully executed: ") << succeeded
                   << " of " << tasks.size() << " operations.\n";
      }
      if (opt::verbose > 2 && failed > 0) {
         std::cout << ui::color::red("Failed to execute: ") << failed << " of "
                   << tasks.size() << " operations.\n";
      }
   }
}
}  // namespace yabr::task
