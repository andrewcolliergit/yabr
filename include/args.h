#ifndef ARGS_H
#define ARGS_H

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace parser_error {
class UnknownOptionException : public std::runtime_error {
public:
   UnknownOptionException(const char c)
       : std::runtime_error("Unknown option: " + std::string(1, c)) {
   }

   UnknownOptionException(const std::string_view& s)
       : std::runtime_error("Unknown option: " + std::string(s)) {
   }
};

class MissingArgException : public std::runtime_error {
public:
   MissingArgException(const char c)
       : std::runtime_error("Missing required argument for option: " +
                            std::string(1, c)) {
   }

   MissingArgException(const std::string_view& s)
       : std::runtime_error("Missing required argument for option: " +
                            std::string(s)) {
   }
};

class DuplicateOptionException : public std::runtime_error {
public:
   DuplicateOptionException(const char c)
       : std::runtime_error("Duplicate option: " + std::string(1, c)) {
   }

   DuplicateOptionException(const std::string_view& s)
       : std::runtime_error("Duplicate option: " + std::string(s)) {
   }
};
}  // namespace parser_error

namespace args {
struct Option {
   char flag = '\0';
   std::string lflag = "";
   std::string desc = "";
   bool seen = false;
   std::string value_var = "";
   bool require_value = false;
   bool optional_value = false;
   std::string value = "";
   std::function<void(const std::string&)> fn = nullptr;
   std::function<bool(std::string_view v)> value_check = nullptr;
};

class Parser {
public:
   std::string program;
   std::string version;
   std::string description;
   std::string usage;

   Parser() = default;
   Parser(int argc, char* argv[]);
   std::vector<std::string> get() const;
   Option* get(const char flag);
   Option* get(const std::string_view long_flag);
   void read_argv(int argc, char* argv[]);
   void add_opt(const Option& opt);
   void read_pipe();
   void help() const;
   void parse();
   void auto_pipe(bool enable);

private:
   bool auto_pipe_ = false;
   bool end_args_ = false;
   size_t lflag_max_;
   std::vector<std::string> argv_;
   std::vector<Option> options_;
   std::vector<std::string> positional_args_;
   std::vector<std::string> piped_args_;
};
}  // namespace args

#endif
