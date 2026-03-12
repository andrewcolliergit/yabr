#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>

// Cross-platform pipe detection setup
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>

#define ISATTY _isatty
#define STDIN_FD 0
#define STDOUT_FD 1
#define GET_CHAR _getch
#include <process.h>
#define GETPID _getpid
#define EDITOR_FALLBACK notepad.exe
#define _CRT_SECURE_NO_WARNINGS
#else  // Linux + MacOS
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#define ISATTY isatty
#define STDIN_FD 0
#define STDOUT_FD 1
#define GET_CHAR get_raw_char
#define GETPID getpid
#define EDITOR_FALLBACK vi
#endif

struct Editor {
   std::string editor;
   std::string fallback;
};

inline Editor get_editor() {
#ifdef _WIN32
   Editor ed = {.fallback = "notepad"};
   char* buf = nullptr;
   size_t len = 0;
   _dupenv_s(&buf, &len, "EDITOR");
   if (buf) {
      ed.editor = buf;
      free(buf);
   }
   return ed;
#else
   const char* e = std::getenv("EDITOR");
   return Editor{.editor = e ? e : "", .fallback = "vi"};
#endif
}

inline void platform_init() {
#ifdef _WIN32
   // Set console code page to UTF-8
   SetConsoleOutputCP(65001);
   SetConsoleCP(65001);
#endif
}

#ifndef _WIN32
inline char get_raw_char() {
   termios oldt, newt;
   tcgetattr(STDIN_FILENO, &oldt);  // save current settings
   newt = oldt;
   newt.c_lflag &= ~(ICANON | ECHO);         // disable line buffering and echo
   tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // apply new settings

   char c = getchar();

   tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // restore old settings
   return c;
}
#endif

inline size_t get_terminal_width() {
#ifdef _WIN32
   CONSOLE_SCREEN_BUFFER_INFO csbi;
   if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
      return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
   struct winsize w;
   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) return w.ws_col;
#endif
   return 80;  // Standard fallback
}

#endif
