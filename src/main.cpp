#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "args.h"
#include "config.h"
#include "path.h"
#include "platform.h"
#include "sort.h"
#include "tasks.h"
#include "temp.h"
#include "ui.h"

using namespace std;

void parse_fmt(const std::string& fmt);
void configure_arg_parser(args::Parser& args);
std::vector<yabr::Source> expand_and_validate_sources(
    const vector<string>& sources);
void sort_sources(std::vector<yabr::Source>& sources);

int main(int argc, char* argv[]) {
   platform_init();
   yabr::opt::color =
       (ISATTY(STDOUT_FD) != 0);  // stdout IS a tty, enable colors

   std::error_code ec;
   yabr::path::cwd = std::filesystem::current_path(ec);
   if (ec) {
      std::cerr << "Unable to resolve current working directory";
      return 1;
   }

   /*
    * 0. Commandline arguments
    */
   args::Parser args;
   configure_arg_parser(args);
   args.read_pipe();
   args.read_argv(argc, argv);
   try {
      args.parse();
   }
   catch (const std::runtime_error& e) {
      std::cerr << args.program << ": " << e.what() << std::endl;
      return 1;
   }

   /*
    * 1. Collect sources
    */
   std::vector<yabr::Source> sources;
   std::vector<std::filesystem::path> source_paths;
   std::vector<std::string> source_list;
   source_list.reserve(args.get().size());

   if (args.get().empty()) {
      source_list.emplace_back(yabr::path::cwd.string());
   }
   else {
      for (const std::string& arg : args.get()) {
         source_list.push_back(arg);
      }
   }

   try {
      sources = expand_and_validate_sources(source_list);
   }
   catch (std::exception& e) {
      return 1;
   }

   /*
    * 2. Set base directory based on the LCA of the valid source paths
    */
   source_paths.reserve(sources.size());
   for (const yabr::Source& src : sources) {
      source_paths.push_back(src.path());
   }
   yabr::set_base_directory(source_paths);

   /*
    * 3. Sort and remove duplicate sources before writing to file
    */
   sort_sources(sources);

   /*
    * 4. Convert sources to strings in preparation for writing to file
    */
   source_list.clear();
   for (const yabr::Source& src : sources) {
      source_list.push_back(yabr::ui::path_str(src.path(), src.is_directory));
   }

   /*
    * 5. Prepare temp file, target list and task list for population.
    *    Write formatted string source_list to the tempfile, edit them in
    *    EDITOR, then read back into target_list. Alternatively if an input file
    *    was provided, skip this and read input_file directly into the
    *    target_list, skipping the temp file and EDITOR.
    */
   yabr::tempfile::TempFile tf;
   std::vector<std::string> target_list;
   std::vector<yabr::task::Task> tasks;

   if (yabr::opt::input_file) {
      yabr::inputfile::read_input_file(*yabr::opt::input_file, target_list);
   }
   else {
      tf.write(source_list);
      tf.edit();
      target_list = tf.read();
   }

   /* 6. Construct and validate an ordered task list.
    *    Preview the list to the user and allow them
    *    to confirm, abort or edit.
    *    The following options will skip this preview:
    *    --exec             User explicitly skipped this preview
    *    --dry-run          Dry run will print in execute()
    *    --input-file FILE  Implies yabr should be non-interactive
    */
   yabr::ui::prompt::PreviewAction action =
       yabr::ui::prompt::PreviewAction::none;
   do {
      try {
         tasks = yabr::task::create_tasks(target_list, sources, ec);
         yabr::task::validate_tasks(tasks);
      }
      catch (std::exception& e) {
         return 1;
      }
      yabr::task::sort::depth_first(tasks);
      tasks = yabr::task::sort::topo_graph(tasks);
      bool change_detected = false;

      // Check if anything was changed
      for (const yabr::task::Task& task : tasks) {
         if (task.target != task.source) {
            change_detected = true;
            break;
         }
      }
      if (!change_detected || yabr::opt::exec || yabr::opt::dry_run ||
          yabr::opt::input_file) {
         break;
      }

      // Preview
      std::cout << "\n";
      yabr::opt::preview = true;
      yabr::task::execute(tasks, false);
      yabr::opt::preview = false;
      std::cout << "\n";

      // Confirm with user
      switch (action = yabr::ui::prompt::preview_action()) {
         case yabr::ui::prompt::PreviewAction::cancel:
            return 0;
         case yabr::ui::prompt::PreviewAction::execute:
            break;
         case yabr::ui::prompt::PreviewAction::reset:
            tf.clear();
            tf.write(source_list);
         case yabr::ui::prompt::PreviewAction::modify:
            tf.edit();
            target_list = tf.read();
         default:
            break;
      }
   } while (action != yabr::ui::prompt::PreviewAction::execute);

   /*
    * 7. Execute tasks
    */
   bool exec = !yabr::opt::dry_run;  // if dry run is true, exec is false
   yabr::task::execute(tasks, exec);

   return 0;
}

// %xn, where x is the digit modifier
// %n  = (0..999) -> 42: "42"
// %4n = (0..999) -> 42: "0042"
// %#n = (0..999) -> 42: "042"
void parse_fmt(const std::string& fmt) {
   size_t num_pos = fmt.find('%');
   if (num_pos == std::string::npos) {
      throw std::runtime_error(
          "Invalid increment format must contain numeric variable %n");
   }
   yabr::opt::incfmt->prefix = fmt.substr(0, num_pos);

   // Get argument x
   std::string x;
   size_t arg_start_pos = num_pos + 1;
   size_t arg_end_pos = 0;
   for (size_t i = arg_start_pos; i < fmt.length(); ++i) {
      char digit = fmt[i];
      if (std::isdigit(digit) || (i == arg_start_pos && digit == '#')) {
         x += digit;
      }
      else if (digit == 'n') {
         arg_end_pos = i;
         break;
      }
      else {
         throw std::runtime_error("Invalid increment format");
      }
   }

   if (arg_end_pos == 0) {
      throw std::runtime_error("Invalid increment format");
   }
   yabr::opt::incfmt->suffix = fmt.substr(arg_end_pos + 1);

   if (x == "#") {
      yabr::opt::incfmt->min_digits = -1;
   }
   else if (x.empty()) {
      yabr::opt::incfmt->min_digits = 0;
   }
   else {
      try {
         yabr::opt::incfmt->min_digits = std::stoi(x);
      }
      catch (...) {
         throw std::runtime_error("Increment width is too large");
      }
   }
}

void configure_arg_parser(args::Parser& args) {
   const size_t VERBOSITY_MIN = 0;
   const size_t VERBOSITY_MAX = 3;
   // Preliminary configurations
   args.auto_pipe(true);
   args.program = "yabr";
   args.version = "v0.1.0";
   args.usage = "usage: " + args.program + " [option] [file ...]";
   // args.description =
   //    "A CLI bulk rename utility that uses your editor to rename files.";

   // Option definitions
   args::Option help = {.flag = 'h',
                        .lflag = "help",
                        .desc = "Print this help message.",
                        .fn = [&args](const std::string&) {
                           args.help();
                           exit(0);
                        }};

   args::Option verbose = {
       .flag = 'v',
       .lflag = "verbose",
       .desc =
           "Enable verbose output. Passing -v without LEVEL will default to "
           "a verbosity level of 2. 0 = No errors or warnings, 1 = Errors "
           "only, 2 = warnings, 3 = info.",
       .value_var = "LEVEL",
       .require_value = false,
       .optional_value = true,
       .fn =
           [&](const std::string& lvl) {
              size_t level;
              if (!lvl.empty()) {
                 level = static_cast<size_t>(std::stoi(lvl));
                 if (level >= VERBOSITY_MIN && level <= VERBOSITY_MAX) {
                    yabr::opt::verbose = level;
                 }
                 else {
                    std::cerr << "Invalid verbosity level" << std::endl;
                    exit(1);
                 }
              }
              else {
                 yabr::opt::verbose = 2;
              }
           },
       .value_check = [](std::string_view v) -> bool {
          if (v.size() != 1) return false;
          char c = v[0];
          return c >= std::to_string(VERBOSITY_MIN)[0] &&
                 c <= std::to_string(VERBOSITY_MAX)[0];
       }};

   args::Option hidden = {
       .flag = 'a',
       .lflag = "show-hidden",
       .desc = "Include hidden files.",
       .fn = [&](const std::string&) { yabr::opt::show_hidden = true; }};
   args::Option force = {
       .flag = 'f',
       .lflag = "force",
       .desc =
           "Force overwriting without prompt. Must be combined with -D to "
           "supress prompt to remove non-empty directories. Mutually exclusive "
           "with -s.",
       .fn = [&](const std::string&) {
          yabr::opt::force = true;

          if (yabr::opt::skip)
             throw std::runtime_error(
                 "Options -f and -s are mutually exclusive.");
       }};

   args::Option dry = {
       .flag = 'n',
       .lflag = "dry",
       .desc =
           "Dry run without renaming any files. This will invoke a verbosity "
           "level of 3.",
       .fn = [&](const std::string&) { yabr::opt::dry_run = true; }};
   args::Option absolute = {
       .flag = 'l',
       .lflag = "absolute-paths",
       .desc = "Display absolute paths.",
       .fn = [&](const std::string&) { yabr::opt::absolute = true; }};

   args::Option recursive = {
       .flag = 'R',
       .lflag = "recursive",
       .desc = "Recursively include files.",
       .fn = [&](const std::string&) { yabr::opt::recursive = true; }};

   args::Option symlinks = {
       .flag = 'L',
       .lflag = "follow-symlinks",
       .desc = "Follow symlinks when traversing directories.",
       .fn = [&](const std::string&) { yabr::opt::follow_symlinks = true; }};

   args::Option strict = {
       .flag = '\0',
       .lflag = "strict",
       .desc =
           "Warnings are elevated to errors and immediately abort the program.",
       .fn = [&](const std::string&) { yabr::opt::strict = true; }};

   args::Option dirs = {
       .flag = 'd',
       .lflag = "dirs-as-files",
       .desc =
           "Treat directories as regular files. Has no effect "
           "when combined with -R.",
       .fn = [&](const std::string&) { yabr::opt::dirs_as_files = true; }};
   args::Option mkdirs = {
       .flag = 'p',
       .lflag = "create-directories",
       .desc =
           "Automatically create intermediary directories as required. "
           "Suppresses prompt.",
       .fn = [&](const std::string&) { yabr::opt::create_directories = true; }};
   args::Option reverse = {
       .flag = 'r',
       .lflag = "reverse",
       .desc = "Reverse the sorting order.",
       .fn = [&](const std::string&) { yabr::opt::reverse = true; }};
   args::Option increment = {
       .flag = 'i',
       .lflag = "increment",
       .desc = "Automatically increment collisions. ex: \"file (1).ext\"",
       .fn = [&](const std::string&) {
          yabr::opt::increment = true;
          parse_fmt(" (%n)");
          if (!yabr::opt::incfmt) {
             yabr::opt::incfmt =
                 yabr::IncrementFmt{};  // initialize with defaults
          }
       }};
   args::Option incfmt = {
       .flag = 'I',
       .lflag = "increment-format",
       .desc =
           "Same as -i, but requires a format where "
           "%n is the number. The variable n can be prepended "
           "with a number to specify the minimum digits (ex. %3n), or "
           "\'#\' to automatically zero-pad (ex. %#n).",
       .value_var = "FMT",
       .require_value = true,
       .fn = [&](const std::string& fmt) {
          yabr::opt::increment = true;
          parse_fmt(fmt);
       }};
   args::Option del = {
       .flag = 'D',
       .lflag = "delete",
       .desc =
           "Suppress deletion prompt. Attempting to delete non-empty "
           "directories will still prompt unless the -f flag is used.",
       .fn = [&](const std::string&) { yabr::opt::del = true; }};
   args::Option debug = {
       .flag = '\0',
       .lflag = "debug",
       .desc = "Enable debug-level logging. Equivalent to --verbose=3",
       .fn = [&](const std::string&) { yabr::opt::verbose = 3; }};
   args::Option version = {.flag = '\0',
                           .lflag = "version",
                           .desc = "Print the build version",
                           .fn = [&](const std::string&) {
                              std::cout << args.program << " version "
                                        << args.version << std::endl;
                              exit(0);
                           }};
   args::Option num_sort = {
       .flag = 'V',
       .lflag = "version-sort",
       .desc =
           "Version sort sources. Useful for correctly ordering numbered "
           "files.",
       .fn = [&](const std::string&) { yabr::opt::numeric_sort = true; }};
   args::Option input_file = {
       .flag = '\0',
       .lflag = "input-file",
       .desc = "Rename sources using targets in FILE.",
       .value_var = "FILE",
       .require_value = true,
       .fn = [&](const std::string& file) {
          std::error_code ec;
          if (!std::filesystem::exists(file, ec)) {
             throw std::filesystem::filesystem_error("Input file", file, ec);
          }
          yabr::opt::input_file = std::filesystem::path(file);
       }};
   args::Option exec = {.flag = 'e',
                        .lflag = "exec",
                        .desc = "Execute rename without preview.",
                        .fn = [&](const std::string&) {
                           yabr::opt::exec = true;
                           if (yabr::opt::dry_run)
                              throw std::runtime_error(
                                  "Options -e and -n are mutually exclusive");
                        }};
   args::Option skip = {
       .flag = 's',
       .lflag = "skip",
       .desc =
           "Automatically skip conflicting tasks. Mutually exclusive with -f.",
       .fn = [&](const std::string&) {
          yabr::opt::skip = true;
          if (yabr::opt::force)
             throw std::runtime_error(
                 "Options -f and -s are mutually exclusive.");
       }};

   // Ordering in help message
   args.add_opt(help);
   args.add_opt(version);
   args.add_opt(verbose);
   args.add_opt(debug);
   args.add_opt(exec);
   args.add_opt(dry);
   args.add_opt(absolute);
   args.add_opt(hidden);
   args.add_opt(dirs);
   args.add_opt(recursive);
   args.add_opt(symlinks);
   args.add_opt(force);
   args.add_opt(skip);
   args.add_opt(del);
   args.add_opt(mkdirs);
   args.add_opt(strict);
   args.add_opt(reverse);
   args.add_opt(num_sort);
   args.add_opt(increment);
   args.add_opt(incfmt);
   args.add_opt(input_file);
}

std::vector<yabr::Source> expand_and_validate_sources(
    const vector<string>& sources) {
   std::error_code ec;
   bool errors_detected = false;
   std::vector<yabr::Source> validated;
   validated.reserve(sources.size());

   auto handle_ec = [&](const std::string& path) {
      if (!ec) return;
      yabr::log::Level level = (yabr::opt::strict) ? yabr::log::Level::error
                                                   : yabr::log::Level::warning;
      yabr::log::log(
          level,
          {.action = "Skipping source", .src = path, .reason = ec.message()});
      if (yabr::opt::strict) throw std::exception();
      errors_detected = true;
      ec.clear();
   };

   // Set directory options
   std::filesystem::directory_options options =
       std::filesystem::directory_options::none;
   if (!yabr::opt::strict) {
      options |= std::filesystem::directory_options::skip_permission_denied;
   }
   if (yabr::opt::follow_symlinks) {
      options |= std::filesystem::directory_options::follow_directory_symlink;
   }

   for (const string& s : sources) {
      std::optional<yabr::Source> src = yabr::Source::create(s, ec);
      handle_ec(s);
      if (!src) continue;

      // Skip hidden files
      if (yabr::is_hidden(src->path())) continue;

      bool treat_as_regular;

      if (src->is_directory) {
         if (yabr::opt::dirs_as_files) {
            treat_as_regular = true;
         }
         else if (src->is_symlink && !yabr::opt::follow_symlinks) {
            treat_as_regular = true;
         }
         else {
            treat_as_regular = false;
         }
      }
      else {
         treat_as_regular = true;
      }

      if (treat_as_regular) {
         validated.push_back(*src);
         continue;
      }

      if (yabr::opt::recursive) {
         std::filesystem::recursive_directory_iterator it(
             std::filesystem::path(s), options, ec);
         handle_ec(s);
         try {
            for (const auto& e : it) {
               if (!e.is_directory() || std::filesystem::is_empty(e)) {
                  if (yabr::is_hidden(e.path())) continue;
                  src = yabr::Source::create(e.path(), ec);
                  handle_ec(e.path().string());
                  if (!src) continue;
                  validated.push_back(*src);
               }
            }
         }
         catch (std::filesystem::filesystem_error& e) {
            ec = e.code();
            handle_ec(e.path1().string());
         }
      }
      else {
         std::filesystem::directory_iterator it(std::filesystem::path(s),
                                                options, ec);
         handle_ec(s);
         try {
            for (const auto& e : it) {
               if (yabr::is_hidden(e.path())) continue;
               handle_ec(e.path().string());
               src = yabr::Source::create(e.path(), ec);
               handle_ec(e.path().string());
               if (!src) continue;
               validated.push_back(*src);
            }
         }
         catch (std::filesystem::filesystem_error& e) {
            ec = e.code();
            handle_ec(e.path1().string());
         }
      }
   }

   if (validated.empty()) {
      yabr::log::log(yabr::log::Level::error, {.action = "No valid sources"});
      throw std::exception();
   }

   // Wait for input if there were warnings, otherwise the user can't read them
   if (yabr::opt::verbose > 1 && !yabr::opt::exec && !yabr::opt::input_file &&
       errors_detected) {
      yabr::ui::prompt::press_any_key_to_continue();
   }

   return validated;
}

void sort_sources(std::vector<yabr::Source>& sources) {
   // Sort in ascending order
   auto to_lower = [](std::string s) {
      std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
         return static_cast<char>(std::tolower(c));
      });
      return s;
   };

   // Sort lexigraphically by default, or numerically if the option is
   // invoked
   if (yabr::opt::numeric_sort) {
      std::sort(sources.begin(), sources.end(),
                [](const yabr::Source& a, const yabr::Source& b) {
                   return yabr::task::sort::natural_compare(
                              a.path().string(), b.path().string()) < 0;
                });
   }
   else {
      std::sort(sources.begin(), sources.end(),
                [&](const yabr::Source& a, const yabr::Source& b) {
                   return to_lower(a.path().string()) <
                          to_lower(b.path().string());
                });
   }

   // Remove dupes
   auto last = std::unique(sources.begin(), sources.end(),
                           [](const yabr::Source& a, const yabr::Source& b) {
                              return a.path() == b.path();
                           });
   sources.erase(last, sources.end());

   // Reverse order if user elected to do so
   if (yabr::opt::reverse) {
      std::reverse(sources.begin(), sources.end());
   }
}
