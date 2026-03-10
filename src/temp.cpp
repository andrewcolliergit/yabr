#include "temp.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>

#include "platform.h"

namespace fs = std::filesystem;
using namespace std;

namespace yabr {
namespace inputfile {
void read_input_file(const std::filesystem::path& p,
                     std::vector<std::string>& list) {
   ifstream ifs;
   ifs.open(p);
   if (!ifs) {
      throw std::runtime_error("Failed to open input file at " + p.string());
   }

   std::string line;
   while (std::getline(ifs, line)) {
      list.emplace_back(line);
   }
   ifs.close();
}
}  // namespace inputfile

namespace tempfile {
std::filesystem::path tempfile::TempFile::generate_temp_path(
    fs::path filepath) {
   fs::path temp_ext("yabr" + std::to_string(GETPID()) + ".tmp");

   if (filepath.empty()) {
      return std::filesystem::temp_directory_path() / temp_ext;
   }
   else {
      return filepath += temp_ext;
   }
}

TempFile::TempFile() : path_(generate_temp_path()) {
   ofstream ofs(path_);
   if (!ofs) {
      throw std::runtime_error("Could not create temp file at " +
                               path_.string());
   }
}

TempFile::~TempFile() {
   std::error_code ec;
   fs::remove(path_, ec);
}

void TempFile::write(const vector<string>& lines) {
   error_code ec;
   ofstream ofs(path_);
   if (!ofs) {
      throw std::runtime_error("Could not open temp file for writing: " +
                               path_.string());
   }
   size_t i;
   for (i = 0; i < lines.size(); i++) {
      ofs << lines[i] << "\n";
   }
}

vector<std::string> TempFile::read() const {
   ifstream ifs(path_);
   if (!ifs) {
      throw std::runtime_error("Could not open temp file for reading: " +
                               path_.string());
   }

   vector<string> targets;
   string line;

   while (std::getline(ifs, line)) {
      targets.emplace_back(line);
   }
   return targets;
}

void TempFile::edit() {
   Editor ed = get_editor();

   std::string cmd = std::string(ed.editor.empty() ? ed.fallback : ed.editor) +
                     " " + path_.string();
   if (std::system(cmd.c_str()) != 0) {
      throw std::runtime_error("Editor exited with an error status");
   }
}

void TempFile::clear() {
   std::ofstream(path_, std::ios::trunc);
}

}  // namespace tempfile
}  // namespace yabr
