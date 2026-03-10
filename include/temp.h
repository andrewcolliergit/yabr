#ifndef TEMP_H
#define TEMP_H

#include <filesystem>
#include <string>
#include <vector>

namespace yabr {
namespace inputfile {
void read_input_file(const std::filesystem::path& p,
                     std::vector<std::string>& list);
}

namespace tempfile {
class TempFile {
   static std::filesystem::path generate_temp_path(
       std::filesystem::path filepath = {});

protected:
   const std::filesystem::path path_;

public:
   TempFile();
   ~TempFile();
   TempFile(const TempFile&) = delete;
   TempFile& operator=(const TempFile&) = delete;

   const std::filesystem::path& path() const {
      return path_;
   }

   std::vector<std::string> read() const;
   void write(const std::vector<std::string>& lines);
   void edit();
   void clear();
};

}  // namespace tempfile
}  // namespace yabr

#endif
