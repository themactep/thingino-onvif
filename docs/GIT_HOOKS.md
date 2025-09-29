# Git Hooks

This document describes the git hooks configured for the thingino-onvif project.

## Pre-commit Hook

The pre-commit hook automatically formats C/C++ source files using `clang-format` before each commit.

### What it does

1. Checks if `clang-format` is installed on your system
2. Identifies all staged C/C++ files (`.c`, `.h`, `.cpp`, `.hpp`, `.cc`, `.cxx`)
3. Formats each file using the project's `.clang-format` configuration
4. Re-stages the formatted files
5. Proceeds with the commit

### Installation

The hook is already installed at `.git/hooks/pre-commit`. If you need to reinstall it or set it up in a fresh clone:

```bash
# The hook file is located at: .git/hooks/pre-commit
# Make sure it's executable:
chmod +x .git/hooks/pre-commit
```

### Requirements

You need to have `clang-format` installed on your system:

**Ubuntu/Debian:**
```bash
sudo apt-get install clang-format
```

**macOS:**
```bash
brew install clang-format
```

**Fedora/RHEL:**
```bash
sudo dnf install clang-tools-extra
```

### Usage

The hook runs automatically when you commit. No manual action is required.

```bash
# Stage your changes
git add src/my_file.c

# Commit (hook will automatically format the file)
git commit -m "Add new feature"
```

### Bypassing the Hook

If you need to bypass the hook for a specific commit (not recommended):

```bash
git commit --no-verify -m "Your commit message"
```

### Formatting Configuration

The formatting rules are defined in `.clang-format` at the project root. The hook uses this configuration automatically.

### Manual Formatting

You can also manually format files without committing:

```bash
# Format a single file
clang-format -i src/my_file.c

# Format all C/C++ files in src/
find src -name '*.c' -o -name '*.h' | xargs clang-format -i
```

### Troubleshooting

**Hook doesn't run:**
- Verify the hook is executable: `ls -l .git/hooks/pre-commit`
- If not executable: `chmod +x .git/hooks/pre-commit`

**clang-format not found:**
- Install clang-format using the instructions above
- Verify installation: `clang-format --version`

**Files not being formatted:**
- Check that files are staged: `git status`
- Verify file extensions match: `.c`, `.h`, `.cpp`, `.hpp`, `.cc`, `.cxx`
- Check `.clang-format` exists in project root

### Notes

- The hook only formats **staged** files, not all files in the repository
- Formatted files are automatically re-staged after formatting
- The hook will fail if `clang-format` is not installed, preventing commits with unformatted code
- This ensures consistent code style across all contributors

