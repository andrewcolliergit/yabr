#include "path.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <system_error>

#include "config.h"

#ifdef _WIN32
#include <io.h>
#define ACCESS _access
#define W_OK 2
#else
#include <unistd.h>
#define ACCESS access
#endif

namespace fs = std::filesystem;

namespace {
/**
 * @brief Converts p to a lexically normal absolute path
 */
fs::path normalize_path(const fs::path& p,
                        const fs::path& anchor,
                        std::error_code& ec) {
   fs::path result = fs::absolute(anchor / p, ec).lexically_normal();

   // If it ends in a slash and isn't the root directory, strip the slash
   if (result.has_relative_path() && !result.has_filename()) {
      result = result.parent_path();
   }
   return result;
}

/**
 * @brief True if user has write permissions for p
 */
bool is_accessible(const fs::path& p) {
   return (ACCESS(p.string().c_str(), W_OK) == 0);
}

/**
 * @brief Returns the nearest existing ancestor of a path
 *
 * @param p must be a normalized absolute path
 */
fs::path closest_parent(fs::path p, std::error_code& ec) {
   do {
      fs::path parent = p.parent_path();
      if (parent == p) return p;  // Root
      p = parent;
   } while (!fs::exists(p, ec) && !ec);

   if (ec)
      return {};
   else
      return p;
}

/**
 * @brief True if a target is empty or full of whitespace
 */
bool is_blank(const fs::path& p) {
   return p.empty() || p.string().find_first_not_of(" \t") == std::string::npos;
}

/**
 * @brief Returns the least common ancestor of two paths.
 */
fs::path find_lca(const fs::path& p1, const fs::path& p2) {
   auto it1 = p1.begin();
   auto it2 = p2.begin();
   fs::path result;

   while (it1 != p1.end() && it2 != p2.end() && *it1 == *it2) {
      result /= *it1;
      ++it1;
      ++it2;
   }
   return result;
}

}  // namespace

namespace yabr {
void set_base_directory(const std::vector<fs::path>& paths) {
   if (paths.empty()) {
      throw std::runtime_error(
          "Unable to compute base directory: No paths detected");
   }

   fs::path common = paths.front().parent_path();
   for (size_t i = 1; i < paths.size(); ++i) {
      common = find_lca(common, paths[i].parent_path());
      if (common.relative_path().empty()) break;
   }
   bool base_directory_exists = fs::exists(common);
   if (!base_directory_exists) {
      throw std::runtime_error("Failed to set base directory: " +
                               common.string());
   }
   else {
      yabr::path::base_directory = common;
   }
}

bool is_hidden(const fs::path& p) {
   return (!yabr::opt::show_hidden && p.filename().string().starts_with('.'));
}

fs::path Path::path() const {
   return path_;
}

std::optional<Source> Source::create(const fs::path& p, std::error_code& ec) {
   fs::path src_path = {};
   bool is_sym, is_dir, exists;

   src_path = normalize_path(p, yabr::path::cwd, ec);
   if (ec) return std::nullopt;

   exists = fs::exists(src_path, ec);
   if (ec) return std::nullopt;
   if (!exists) {
      ec = std::make_error_code(std::errc::no_such_file_or_directory);
      return std::nullopt;
   }

   is_sym = fs::is_symlink(src_path, ec);
   if (ec) return std::nullopt;

   if (is_sym) {
      is_dir = (fs::status(src_path, ec).type() == fs::file_type::directory);
   }
   else {
      is_dir = fs::is_directory(src_path, ec);
   }
   if (ec) return std::nullopt;

   // If a directory, also check it's permissions to see if it's traversable
   if (is_dir && !is_accessible(src_path)) {
      ec = std::make_error_code(std::errc::permission_denied);
      return std::nullopt;
   }

   // For directories used as containers, check the dir itself is traversable.
   // For files and symlinks, check the parent is writable — that's what
   // determines whether a rename is possible.
   if (is_dir) {
      if (!is_accessible(src_path)) {
         ec = std::make_error_code(std::errc::permission_denied);
         return std::nullopt;
      }
   }
   else {
      if (!is_accessible(src_path.parent_path())) {
         ec = std::make_error_code(std::errc::permission_denied);
         return std::nullopt;
      }
   }

   return Source(src_path, is_dir, is_sym);
}

std::optional<Target> Target::create(const fs::path& p, std::error_code& ec) {
   if (is_blank(p)) {
      return Target({}, false, true);
   }

   fs::path trg_path, nearest_ancestor;

   trg_path = normalize_path(p, yabr::path::base_directory, ec);
   if (ec) return std::nullopt;

   nearest_ancestor = closest_parent(trg_path, ec);
   if (ec) return std::nullopt;

   if (!is_accessible(nearest_ancestor)) {
      ec = std::make_error_code(std::errc::permission_denied);
      return std::nullopt;
   }

   return Target(trg_path, (trg_path.parent_path() != nearest_ancestor), false);
}

}  // namespace yabr
