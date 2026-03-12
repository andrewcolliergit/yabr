#ifndef PATH_H
#define PATH_H

#include <filesystem>
#include <optional>
#include <system_error>
#include <vector>

namespace yabr {
/**
 * @brief Sets the global base directory to least common ancestor of paths
 */
void set_base_directory(const std::vector<std::filesystem::path>& paths);

bool is_hidden(const std::filesystem::path& p);

/**
 * @class Path
 * @brief Base class for yabr::Source and yabr::Target
 * Contains shared members and methods.
 *
 */
class Path {
protected:
   /**
    * @brief Strictly a lexically normal absolute path
    */
   std::filesystem::path path_ = {};

   Path(const std::filesystem::path& p) : path_(p) {};

public:
   /**
    * @brief Getter for the contained std::filesystem::path object
    */
   std::filesystem::path path() const;
};

/**
 * @class Source
 * @brief Represents the path of a rename source
 *
 */
class Source : public Path {
   Source(const std::filesystem::path& p, bool is_dir, bool is_sym)
       : Path(p), is_directory(is_dir), is_symlink(is_sym) {};

public:
   /**
    * @brief True if path_ is a directory
    */
   bool is_directory = false;
   /**
    * @brief True if path_ is a symlink
    */
   bool is_symlink = false;
   /**
    * @brief Represents the path of a rename source
    * Returns nullopt if the source is invalid for any reason
    *
    * @param p is the path of the source to construct
    * @param ec is an error code that will be set on failure
    */
   static std::optional<Source> create(const std::filesystem::path& p,
                                       std::error_code& ec);
};

/**
 * @class Target
 * @brief Represents the path of a rename target
 *
 */
class Target : public Path {
   Target(const std::filesystem::path& p, bool needs_parent, bool req_del)
       : Path(p), needs_parent(needs_parent), requests_deletion(req_del) {};

public:
   /**
    * @brief True if one or more path ancestors do not exist
    */
   bool needs_parent = false;
   /**
    * @brief True if the target is blank, indicating the corresponding source
    * has been marked for deletion.
    */
   bool requests_deletion = false;
   /**
    * @brief Represents the path of a rename target
    * Returns nullopt if the target is invalid for any reason
    *
    * @param p is the path of the target to construct
    * @param ec is an error code that will be set on failure
    */
   static std::optional<Target> create(const std::filesystem::path& p,
                                       std::error_code& ec);
};

}  // namespace yabr

#endif
