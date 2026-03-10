/**
 * @file config.h
 * @brief Contains global state such as options and base_directory
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>
#include <optional>

namespace yabr {
struct IncrementFmt {
   std::string prefix = " (";
   std::string suffix = ")";
   int min_digits = 0;  // n = -1: dynamically pad with leading zeros
};

namespace path {
inline std::filesystem::path cwd = std::filesystem::current_path();
inline std::filesystem::path base_directory = cwd;
}  // namespace path

namespace opt {
inline bool color = false;
inline size_t verbose = 1;
inline bool dry_run = false;
inline bool recursive = false;
inline bool exec = false;
inline bool absolute = false;
inline bool show_hidden = false;
inline bool follow_symlinks = false;
inline bool strict = false;
inline bool preview = false;
inline bool skip = false;
inline bool dirs_as_files = false;
inline bool del = false;
inline bool force = false;
inline bool create_directories = false;
inline bool reverse = false;
inline bool increment = false;
inline bool numeric_sort = false;
inline size_t leading_zeros = false;
inline std::optional<IncrementFmt> incfmt = std::nullopt;
inline std::optional<std::filesystem::path> input_file = std::nullopt;
}  // namespace opt
}  // namespace yabr

#endif
