# yabr

**yet another bulk renamer** — a CLI tool that uses your text editor to rename files.

Run `yabr` on a directory opens your `$EDITOR` with a list of filenames. Edit the names, save, and quit. yabr renames everything accordingly. That's it.

## Features

- Uses your `$EDITOR` — vim, neovim, VS Code, whatever you prefer
- Handles rename cycles safely (A→B, B→A works correctly)
- Validates and previews all tasks by default
- Allows for edits to be made after preview
- Recursive directory traversal
- Auto-incrementing collision resolution
- Dry run mode to preview changes before committing
- Delete files by blanking out lines
- Pipe-friendly — works with `find`, `ls`, and other tools
- Cross-platform: Linux, macOS, Windows

## Installation

### Linux / macOS

**From a release binary:**

Download the latest binary from the [releases page](../../releases), make it executable, and move it somewhere in your PATH:

```bash
chmod +x yabr
mv yabr /usr/local/bin/
```

**Build from source:**

```bash
git clone https://github.com/andrewcolliergit/yabr.git
cd yabr
make
```

Requires a C++20 compiler (GCC 11+, Clang 13+, or MSVC 2022+) and CMake 3.20+.

### Windows

Download `yabr.exe` from the [releases page](../../releases) and place it in a directory that's in your `PATH`, such as `C:\Program Files\yabr\`. Add that directory to your PATH via System Environment Variables if needed.

Set your preferred editor:

```cmd
setx EDITOR "code --wait"
```

If `EDITOR` is not set, yabr falls back to Notepad.

## Usage

```
yabr [options] [file ...]
```

### Passing paths to yabr

yabr has syntax similar to `ls`. If you run yabr on a single directory, it will use the files in that directory. If you pass it multiple directories, it will return all contents. The path will be relative to the least common ancestor unless `-l` is passed to display long (absolute) path.
yabr also accepts piped paths. This is useful for piping in from `find`, `grep`, `sort`, etc.

Run on the current directory:

```bash
yabr
```

Run on specific files or directories:

```bash
yabr src/ include/
yabr file1.txt file2.txt file3.txt
```

Pipe from another command:

```bash
find . -name "*.jpg" | yabr
ls *.txt | yabr
grep -lR "pattern" ./* | yabr
```

**A note on piping from `ls`:** There is rarely a reason to do this. yabr works with shell globbing and has the same `-d`, `-R`, `-L` options that `ls` has.
The problem with piping into yabr with something like `ls -R` is that you're piping headers into yabr which it doesn't know how to handle.
It's better to use find with the appropriate options if you want to do something like, for example, run yabr on files matching specific criteria that are sorted by file size.

```bash
yabr *.jpg
```

### Renaming and deleting

yabr is designed to be as safe and flexible as possible by default. Files passed to yabr will open in your editor. To rename, simply change the filename.
Files can be deleted by deleting the name, leaving the line blank. ***The total number of lines cannot change.*** yabr will abort if it detects that any lines have been added or removed.

Files can be renamed to anything, anywhere. If the user has permissions, yabr will make it work.

yabr works in the following way:
- Validates all sources passed to it. If a source doesn't exist or the user doesn't have permission to rename or delete it, the source is omitted. This warning is silent by default but can be seen using the `-v` flag. yabr will abort immediately if the `--strict` option is invoked.
- Evaluates all changes the user made. yabr checks to make sure the target path is valid and the user has permissions to move the file there.
- Previews all the proposed actions to the user. The user can then go back and modify their changes, reset the sources to their original names, abort everything, or execute the tasks.

#### Collisions
yabr will detect if a file will be renamed to a path that either already exists or will exist as the result of another rename.
If a collision is detected, yabr will prompt the user to resolve it in one of three ways:
- Overwrite: This overwrites the source with the target. The user can suppress these prompts by using the `-f` flag. ***Forcing overwrites is discouraged*** especially when renaming multiple sources to the same target because the execution order will determine which get deleted, and it can be difficult or impossible to understand what order yabr will rename files due to its underlying task sorting algorithm.
- Skip: This simply skips the rename. This prompt can be suppressed with the `-s` flag.
- Increment: This increments the file name using the default increment format. If using the `-i` or `-I[FMT]` flag, any collisions will be automatically incremented. The behavior of using the `-i` flag and saying "increment all" when prompted about a collision differs slightly, as `-i` will increment all the colliding files, including the initial one, while incrementing collisions on a case-by-case basis will only increment the files that came after the initial one. This is intentional, as the former is a tool for enumerating files while the other is used to resolve naming conflicts without skipping the operation or overwriting the target. 

When prompted to address a collision, the user can specify that they want their choice to be applied "to all". This refers to all issues *of type collision* and does not apply to other issues below. Each issue can be handled uniquely even when using "Apply to all" responses.

#### Path name resolution (missing parents)
yabr will detect if a user has renamed a source to a target who's parent directory(s) do not yet exist. If they don't, yabr will ask unless the `-p` flag was given, indicating that directories should be automatically created to satisfy the target paths. When prompted the user can choose to resolve each case individually or apply their selection to all issues of this type. This is true for all issues.

#### Deleting a file or empty directory
yabr will detect that the user apparently requested a file to be deleted by blanking the line and ask if they want to do that. Passing the `-D` option will suppress this prompt.

#### Deleting a non-empty directory
If you try to delete a non-empty directory, yabr will warn as above and request confirmation. Passing both the `-D` and `-f` flags are required to suppress this prompt, or alternatively, you can respond "yes to all".


## Examples

**Rename files in a directory:**

```bash
yabr ~/photos/
```

Your editor opens with something like:

```
IMG_0001.jpg
IMG_0002.jpg
IMG_0003.jpg
```

Edit the names, save, and quit:

```
vacation_beach.jpg
vacation_sunset.jpg
vacation_dinner.jpg
```

Done.

---

**Recursively rename with a dry run first:**

```bash
yabr -Rn ~/project/
```

---

**Rename files and auto-increment duplicates:**

```bash
yabr -i *.log
```

If multiple files are renamed to the same target, yabr appends ` (1)`, ` (2)`, etc. automatically.

---

**Custom increment format:**

```bash
yabr -I "_%3n" *.jpg
# produces: photo_001.jpg, photo_002.jpg, photo_003.jpg

yabr -I "_%#n" *.jpg
# auto zero-pads based on group size
```

---

**Delete files by blanking lines:**

```bash
yabr -D ~/downloads/
```

In your editor, blank out any line to delete that file. yabr will prompt before deleting non-empty directories unless -f was also passed.

---

**Move files into a new directory structure:**

```bash
yabr -p *.jpg
```

Edit a filename to include a new path like `2024/vacation/photo.jpg`. With `-p`, yabr creates any intermediate directories as needed.

## Options
| Flag | Long flag | Description |
|------|-----------|-------------|
|`-h`| `--help`|Print this help message.|
| |`--version`|Print the build version|
|`-v`|`--verbose`|Enable verbose output. Passing -v without LEVEL will default to a verbosity level of 2. 0 = No errors or warnings, 1 = Errors only, 2 = warnings, 3 = info.|
| |`--debug`|Enable debug-level logging. Equivalent to --verbose=3|
|`-e`    |`--exec`|Execute rename without preview.|
|`-n`    |`--dry`|Dry run without renaming any files. This will invoke a verbosity level of 3.|
|`-l`    |`--absolute-paths`|Display absolute paths.|
|`-a`|`--show-hidden`|Include hidden files.|
|`-d`|`--dirs-as-files`|Treat directories as regular files. Has no effect when combined with -R.|
|`-R`|`--recursive`|Recursively include files.|
|`-L`|`--follow-symlinks`|Follow symlinks when traversing directories.|
|`-f`|`--force`|Force overwriting without prompt. Must be combined with -D to supress prompt to remove non-empty directories. Mutually exclusive with -s.|
|`-s`|`--skip`|Automatically skip conflicting tasks. Mutually exclusive with -f.|
|`-D`|`--delete`|Suppress deletion prompt. Attempting to delete non-empty directories will still prompt unless the -f flag is used.|
|`-p`|`--create-directories`|Automatically create intermediary directories as required. Suppresses prompt.|
|  |`--strict`|Warnings are elevated to errors and immediately abort the program.|
|`-r`|`--reverse`|Reverse the sorting order.|
|`-V`|`--version-sort`|Version sort sources. Useful for correctly ordering numbered files.|
|`-i`|`--increment`|Automatically increment collisions. ex: "file (1).ext"|
|`-I`|`--increment-format=FMT`|Same as -i, but requires a format where %n is the number. The variable n can be prepended with a number to specify the minimum digits (ex. %3n), or '#' to automatically zero-pad (ex. %#n).|
| |`--input-file=FILE`|Rename sources using targets in FILE.|

### Increment Format

The `-I` flag accepts a format string where `%n` is replaced by the increment number:

| Format | Example output |
|--------|---------------|
| `_%n` | `file_1.ext`, `file_2.ext` |
| `_%3n` | `file_001.ext`, `file_002.ext` |
| `_%#n` | auto zero-pads based on group size |
| ` (%n)` | `file (1).ext` (default for `-i`) |

## Notes

- The number of lines in the editor must match the number of input files. Do not add or remove lines.
- Rename cycles (A→B, B→A) are handled automatically using temporary files.
- Hidden files (dotfiles) are excluded by default. Use `-a` to include them.

## License

MIT
