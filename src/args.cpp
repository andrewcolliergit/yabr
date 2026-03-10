#include "args.h"

#include <cassert>
#include <cctype>
#include <iostream>
#include <sstream>

#include "platform.h"

namespace args {

void print_wrapped(std::string_view text, size_t padding = 0) {
   size_t width = get_terminal_width();

   // If padding is 0, we use the full width (minus a small margin).
   // Otherwise, we use the space remaining after the padding.
   size_t target_width = (width > padding + 10) ? width - padding - 2 : 30;

   std::stringstream ss(std::string{text});
   std::string word;
   std::string line;
   bool is_first_line = true;

   while (ss >> word) {
      if (line.length() + word.length() + 1 > target_width) {
         // Only apply indent-padding if we have already moved past the first
         // line OR if we are doing a full-width wrap (padding == 0)
         if (!is_first_line && padding > 0)
            std::cout << std::string(padding, ' ');

         std::cout << line << "\n";
         line = word;
         is_first_line = false;
      }
      else {
         if (!line.empty()) line += " ";
         line += word;
      }
   }

   if (!line.empty()) {
      if (!is_first_line && padding > 0) std::cout << std::string(padding, ' ');
      std::cout << line << "\n";
   }
}

/*
 * Alternate constuctor that automatically reads argv from main
 */
Parser::Parser(int argc, char* argv[]) : Parser::Parser() {
   Parser::read_argv(argc, argv);
}

/*
 * Reads in argv
 */
void Parser::read_argv(int argc, char* argv[]) {
   if (!argv_.empty()) {
      argv_.clear();
   }

   argv_.reserve(argc - 1);

   if (program.empty()) {
      program = argv[0];
   }

   for (int i = 1; i < argc; ++i) {
      argv_.emplace_back(argv[i]);
   }
}

/*
 * Getter for positional arguments
 */
std::vector<std::string> Parser::get() const {
   return positional_args_;
}

/*
 * Getter for a short flag
 */
Option* Parser::get(const char flag) {
   for (Option& o : options_) {
      if (o.flag != '\0' && o.flag == flag) {
         return &o;
      }
   }
   return nullptr;
}

/*
 * Getter for a long flag
 */
Option* Parser::get(const std::string_view lflag) {
   for (Option& o : options_) {
      if (!o.lflag.empty() && o.lflag == lflag) {
         return &o;
      }
   }
   return nullptr;
}

/*
 * Adds an option
 */
void Parser::add_opt(const Option& opt) {
   assert(opt.flag != '\0' ||
          (!opt.lflag.empty() && "Option must have a long or short flag"));

   assert(opt.require_value || opt.optional_value ||
          (opt.value_var.empty() &&
           "Option must have a named value variable if required"));
   options_.emplace_back(opt);
   if (opt.lflag.length() > lflag_max_) lflag_max_ = opt.lflag.length();
}

/*
 * Reads in from pipe
 */
void Parser::read_pipe() {
   if (ISATTY(STDIN_FD) || !piped_args_.empty()) return;

   std::string line;
   while (std::getline(std::cin, line)) {
      if (line.empty()) continue;

      // Handle Windows-style line endings (\r\n) if on Linux/Mac
      if (line.back() == '\r') line.pop_back();

      piped_args_.emplace_back(std::move(line));
   }
}

void Parser::parse() {
   std::string_view arg, val;
   bool pipe_inserted = false;
   Option* opt = nullptr;

   for (size_t i = 0; i < argv_.size(); ++i) {
      arg = argv_[i];
      // End of args symbol encountered
      if (arg == "--") {
         end_args_ = true;
      }
      // Pipe into positional args
      else if (arg == "-") {
         if (!piped_args_.empty()) {
            positional_args_.insert(positional_args_.end(), piped_args_.begin(),
                                    piped_args_.end());
            pipe_inserted = true;
         }
      }
      // long flag
      else if (!end_args_ && arg.starts_with("--")) {
         arg.remove_prefix(2);
         size_t eq_pos = arg.find('=');

         // If an equals sign was found, set the value to the string after it
         if (eq_pos != std::string::npos) {
            val = arg.substr(eq_pos + 1);
            arg = arg.substr(0, eq_pos);
         }
         else {
            val = "";
         }

         // Attempt to match opt now that '=' embedded value has been separated
         opt = Parser::get(arg);

         // Unknown option
         if (!opt) {
            throw parser_error::UnknownOptionException(arg);
         }

         // Required value missing
         if (opt->require_value && val.empty() && i + 1 >= argv_.size()) {
            throw parser_error::MissingArgException(arg);
         }

         // Duplicate option
         if (opt->seen) {
            throw parser_error::DuplicateOptionException(arg);
         }

         // Confirm that the option was seen
         opt->seen = true;

         // Skip the value logic if the option doesn't take a value
         if (opt->require_value) {
            // Set value
            opt->value = (eq_pos == std::string::npos) ? argv_[++i] : val;
         }
         else if (opt->optional_value) {
            opt->value = val;  // empty if no = used, that's fine
         }

         // Call function
         if (opt->fn) {
            opt->fn(opt->value);
         }
      }
      // short flag
      else if (!end_args_ && arg.starts_with("-")) {
         arg.remove_prefix(1);
         for (size_t j = 0; j < arg.size(); ++j) {
            opt = Parser::get(arg[j]);
            val = arg.substr(j + 1);

            // Unknown option
            if (!opt) {
               throw parser_error::UnknownOptionException(arg[j]);
            }

            // Error on duplicates
            if (opt->seen) {
               throw parser_error::DuplicateOptionException(arg[j]);
            }

            // Required value missing
            if (opt->require_value && val.empty() && i + 1 >= argv_.size()) {
               throw parser_error::MissingArgException(arg[j]);
            }

            // Confirm that the option was seen
            opt->seen = true;

            // Get required value
            if (opt->require_value) {
               // Set value
               opt->value = (val.empty()) ? argv_[++i] : val;
               if (opt->fn) opt->fn(opt->value);  // Call function
               break;  // Skip any remaining characters
            }

            // FIXME: Untested
            if (opt->optional_value) {
               // If the option has a validator, only consume the remainder as a
               // value when it passes. Otherwise leave it for the next
               // iteration so characters like 'R' are parsed as flags, not
               // values.
               bool consume =
                   !val.empty() && (!opt->value_check || opt->value_check(val));
               if (consume) {
                  opt->value = val;
                  if (opt->fn) opt->fn(opt->value);
                  break;  // consumed the rest of the arg
               }
               if (opt->fn) opt->fn(opt->value);
               continue;
            }
            if (opt->fn) opt->fn(opt->value);  // plain boolean flag
         }
      }
      // Positional argument
      else {
         positional_args_.emplace_back(arg);
      }
   }

   // Insert piped args at the begining of the positional args list
   // if they weren't already added in a specific position within argv
   if (auto_pipe_ && !pipe_inserted && !piped_args_.empty()) {
      positional_args_.insert(positional_args_.begin(), piped_args_.begin(),
                              piped_args_.end());
   }
}

/*
 * Parse argv and set known options.
 * Also insert piped args, if they exist.
 */
/*
void Parser::parse() {
   std::string_view arg, val;
   bool pipe_inserted = false;
   Option* opt = nullptr;

   for (size_t i = 0; i < argv_.size(); ++i) {
      arg = argv_[i];
      // End of args symbol encountered
      if (arg == "--") {
         end_args_ = true;
      }
      // Pipe into positional args
      else if (arg == "-") {
         if (!piped_args_.empty()) {
            positional_args_.insert(positional_args_.end(), piped_args_.begin(),
                                    piped_args_.end());
            pipe_inserted = true;
         }
      }
      // long flag
      else if (!end_args_ && arg.starts_with("--")) {
         arg.remove_prefix(2);
         size_t eq_pos = arg.find('=');

         // If an equals sign was found, set the value to the string after it
         if (eq_pos != std::string::npos) {
            val = arg.substr(eq_pos + 1);
            arg = arg.substr(0, eq_pos);
         }
         else {
            val = "";
         }

         // Attempt to match opt now that '=' embedded value has been separated
         opt = Parser::get(arg);

         // Unknown option
         if (!opt) {
            throw parser_error::UnknownOptionException(arg);
         }

         // Required value missing
         if (opt->require_value && val.empty() && i + 1 >= argv_.size()) {
            throw parser_error::MissingArgException(arg);
         }

         // Duplicate option
         if (opt->seen) {
            throw parser_error::DuplicateOptionException(arg);
         }

         // Confirm that the option was seen
         opt->seen = true;

         // Skip the value logic if the option doesn't take a value
         if (opt->require_value) {
            // Set value
            opt->value = (eq_pos == std::string::npos) ? argv_[++i] : val;
         }
         else if (opt->optional_value) {
            opt->value = val;  // empty if no = used, that's fine
         }

         // Call function
         if (opt->fn) {
            opt->fn(opt->value);
         }
      }
      // short flag
      else if (!end_args_ && arg.starts_with("-")) {
         arg.remove_prefix(1);
         for (size_t j = 0; j < arg.size(); ++j) {
            opt = Parser::get(arg[j]);
            val = arg.substr(j + 1);

            // Unknown option
            if (!opt) {
               throw parser_error::UnknownOptionException(arg[j]);
            }

            // Error on duplicates
            if (opt->seen) {
               throw parser_error::DuplicateOptionException(arg[j]);
            }

            // Required value missing
            if (opt->require_value && val.empty() && i + 1 >= argv_.size()) {
               throw parser_error::MissingArgException(arg[j]);
            }

            // Confirm that the option was seen
            opt->seen = true;

            // Get required value
            if (opt->require_value) {
               // Set value
               opt->value = (val.empty()) ? argv_[++i] : val;
               if (opt->fn) opt->fn(opt->value);  // Call function
               break;  // Skip any remaining characters
            }

            // FIXME: Untested
            if (opt->optional_value) {
               opt->value = val;  // may be empty, that's fine
               if (opt->fn) opt->fn(opt->value);
               if (!val.empty()) break;  // consumed the rest of the arg
            }
            if (opt->fn) opt->fn(opt->value);  // Call function
         }
      }
      // Positional argument
      else {
         positional_args_.emplace_back(arg);
      }
   }

   // Insert piped args at the begining of the positional args list
   // if they weren't already added in a specific position within argv
   if (auto_pipe_ && !pipe_inserted && !piped_args_.empty()) {
      positional_args_.insert(positional_args_.begin(), piped_args_.begin(),
                              piped_args_.end());
   }
}
*/

void Parser::help() const {
   std::cout << usage << "\n";

   // Wrap the main program description at full width (0 padding)
   if (!description.empty()) {
      print_wrapped(description, 0);
   }
   std::cout << "\n";

   const size_t PAD = 4;
   std::vector<std::string> flag_str;
   flag_str.resize(options_.size());
   size_t max_flag_length = 0;

   for (size_t i = 0; i < options_.size(); ++i) {
      std::string this_flag_str;
      if (options_[i].flag != '\0') {
         this_flag_str += "-";
         this_flag_str += options_[i].flag;
      }
      else {
         this_flag_str += "  ";
      }

      // Separator logic
      if (options_[i].flag != '\0' && !options_[i].lflag.empty())
         this_flag_str += "," + std::string(PAD, ' ');
      else
         this_flag_str += std::string(PAD + 1, ' ');

      // Long flag logic
      if (!options_[i].lflag.empty()) {
         this_flag_str += "--" + options_[i].lflag;
         if (options_[i].require_value)
            this_flag_str += "=" + options_[i].value_var;
      }

      this_flag_str += std::string(PAD, ' ');

      if (this_flag_str.length() > max_flag_length) {
         max_flag_length = this_flag_str.length();
      }
      flag_str[i] = this_flag_str;
   }

   for (size_t i = 0; i < options_.size(); ++i) {
      std::cout << std::string(PAD, ' ') << flag_str[i];

      std::cout << std::string(max_flag_length - flag_str[i].length(), ' ');
      // Wrap the option-specific description at the column
      print_wrapped(options_[i].desc, max_flag_length + PAD);
   }
}

void Parser::auto_pipe(bool enable) {
   auto_pipe_ = enable;
}

}  // namespace args
