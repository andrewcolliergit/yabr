#include "ui.h"

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>

#include "config.h"
#include "platform.h"

using namespace std;

namespace {
char get_char(const char* msg) {
   char c;
   cout << msg << flush;
   c = static_cast<char>(GET_CHAR());
   cout << endl;
   return c;
}
}  // namespace

namespace yabr::ui {
std::string path_str(const std::filesystem::path& p, bool dir) {
   std::string pstr = (opt::absolute)
                          ? p.string()
                          : p.lexically_relative(path::base_directory).string();
   if (dir) pstr += std::filesystem::path::preferred_separator;
   return pstr;
}

namespace color {
std::string red(const std::string& s) {
   return (yabr::opt::color) ? "\033[31m" + s + "\033[0m " : s;
}

std::string green(const std::string& s) {
   return (yabr::opt::color) ? "\033[32m" + s + "\033[0m " : s;
}

std::string yellow(const std::string& s) {
   return (yabr::opt::color) ? "\033[33m" + s + "\033[0m " : s;
}

std::string blue(const std::string& s) {
   return (yabr::opt::color) ? "\033[34m" + s + "\033[0m " : s;
}

}  // namespace color

namespace prompt {
bool yn(const char* msg) {
   char c = get_char(msg);
   return (c == 'y' || c == 'Y');
}

YNAction ynall() {
   while (true) {
      char c = get_char("[yn] this | [YN] all");
      switch (c) {
         case 'y':
            return YNAction::yes;
         case 'Y':
            return YNAction::yesall;
         case 'n':
            return YNAction::no;
         case 'N':
            return YNAction::noall;
         default:
            cerr << "Invalid option\n";
      }
   }
}

PreviewAction preview_action() {
   while (true) {
      char c = get_char("[E]xecute [c]ancel [m]odify [r]eset ");
      switch (c) {
         case 'E':
            return PreviewAction::execute;
         case 'c':
            return PreviewAction::cancel;
         case 'm':
            return PreviewAction::modify;
         case 'r':
            return PreviewAction::reset;
         default:
            cerr << "Invalid option\n";
      }
   }
}

std::string issue_label(size_t num, size_t of) {
   std::string label =
       "Issue [" + std::to_string(num) + "/" + std::to_string(of) + "]";
   return color::yellow(label);
}

void press_any_key_to_continue() {
   CursorGuard cg;  // Hide cursor
   get_char("\n(Press any key to continue)");
}

YNAction mkparent(std::string_view tgt,
                  std::string_view proposed_parent,
                  size_t num,
                  size_t of) {
   cout << issue_label(num, of) << "  Create parent \"" << proposed_parent
        << "\" to satisfy target \"" << tgt << "\"?  ";
   return ynall();
}

YNAction rm_dir(std::string_view src, size_t num, size_t of) {
   cout << issue_label(num, of) << "  Delete empty directory \"" << src
        << "\"?  ";
   return ynall();
}

YNAction rm_file(std::string_view src, size_t num, size_t of) {
   cout << issue_label(num, of) << "  Delete \"" << src << "\"?  ";
   return ynall();
}

YNAction rm_full(std::string_view src, size_t num, size_t of) {
   cout << issue_label(num, of) << "  Delete directory \"" << src
        << "\" and all its contents?  ";
   return ynall();
}

CollisionResponse collision(std::string_view src,
                            std::string_view tgt,
                            size_t num,
                            size_t of) {
   cout << issue_label(num, of) << "  \"" << src << "\" -> \"" << tgt << "\"\n";
   while (true) {
      char c = get_char(
          "[o]verwrite  [i]ncrement [s]kip  [O]verwrite all  [I]ncrement all  "
          "[S]kip all ");
      switch (c) {
         case 'o':
            cout << "Target \"" << tgt << "\" will be overwritten...\n";
            return CollisionResponse::overwrite;
         case 'O':
            cout << "Target \"" << tgt
                 << "\" and all remaining conflicts will be overwritten...\n";
            return CollisionResponse::overwrite_all;
         case 'i':
            cout << "Target \"" << tgt
                 << "\" will be incremented to prevent data loss...\n";
            return CollisionResponse::increment;
         case 'I':
            cout << "Target \"" << tgt
                 << "\" and all remaining conflicts will be incremented to "
                    "prevent data loss...\n";
            return CollisionResponse::increment_all;
         case 's':
            cout << "Target \"" << tgt
                 << "\" will be skipped to prevent data loss...\n";
            return CollisionResponse::skip;
         case 'S':
            cout << "Target \"" << tgt
                 << "\" and all remaining conflicts will be skipped to prevent "
                    "data loss...\n";
            return CollisionResponse::skip_all;
         default:
            cerr << "Invalid option\n";
      }
   }
}

}  // namespace prompt
}  // namespace yabr::ui

namespace yabr::log {
void log(Level level, LogParams params) {
   // Filter based on verbosity
   if (!opt::preview || opt::dry_run) {
      if (yabr::opt::verbose == 0) return;
      if (level == Level::success && yabr::opt::verbose < 3) return;
      if (level == Level::info && yabr::opt::verbose < 3) return;
      if (level == Level::warning && yabr::opt::verbose < 2) return;
   }

   std::string label;
   switch (level) {
      case Level::success:
         label = yabr::ui::color::green("Success");
         break;
      case Level::preview:
         label = yabr::ui::color::blue("Preview");
         break;
      case Level::info:
         label = yabr::ui::color::blue("Info");
         break;
      case Level::dryrun:
         label = yabr::ui::color::blue("Simulated");
         break;
      case Level::warning:
         label = yabr::ui::color::yellow("Warning");
         break;
      case Level::error:
         label = yabr::ui::color::red("Error");  // red
         break;
      default:
         break;
   }
   // Level
   cerr << label;

   if (params.action) {
      cerr << *params.action;
   }
   if (params.action && params.src) {
      cerr << ": ";
   }
   if (params.src) {
      cerr << "\"" << *params.src << "\"";
   }
   if (params.tgt) {
      cerr << " -> \"" << *params.tgt << "\"";
   }
   if (params.reason) {
      cerr << ": " << *params.reason;
   }
   cerr << "\n";
}
}  // namespace yabr::log
