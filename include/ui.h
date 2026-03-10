#ifndef UI_H
#define UI_H

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace yabr::log {
enum class Level { success, info, preview, dryrun, warning, error };

struct LogParams {
   std::optional<std::string_view> action = std::nullopt;
   std::optional<std::string_view> src = std::nullopt;
   std::optional<std::string_view> tgt = std::nullopt;
   std::optional<std::string_view> reason = std::nullopt;
};

void log(Level level, LogParams params = {});
}  // namespace yabr::log

namespace yabr::ui {
std::string path_str(const std::filesystem::path& p, bool dir = false);

namespace color {
std::string red(const std::string& s);
std::string green(const std::string& s);
std::string yellow(const std::string& s);
std::string blue(const std::string& s);
}  // namespace color

struct CursorGuard {
   CursorGuard() {
      std::cout << "\033[?25l";
   }

   ~CursorGuard() {
      std::cout << "\033[?25h";
   }
};

namespace prompt {
enum class PreviewAction { none, execute, cancel, reset, modify };
enum class CollisionResponse {
   skip,
   skip_all,
   overwrite,
   overwrite_all,
   increment,
   increment_all
};
enum class YNAction { none, yes, no, yesall, noall };
bool yn(const char* msg);
YNAction ynall();
PreviewAction preview_action();
void press_any_key_to_continue();
YNAction rm_full(std::string_view src, size_t num, size_t of);
YNAction rm_dir(std::string_view src, size_t num, size_t of);
YNAction rm_file(std::string_view src, size_t num, size_t of);
CollisionResponse collision(std::string_view src,
                            std::string_view tgt,
                            size_t num,
                            size_t of);
YNAction mkparent(std::string_view tgt,
                  std::string_view proposed_parent,
                  size_t num,
                  size_t of);

}  // namespace prompt
}  // namespace yabr::ui

#endif
