#!/bin/bash
#
# Setup git hooks for the thingino-onvif project
# Run this script after cloning the repository to install git hooks
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Setting up git hooks for thingino-onvif...${NC}"
echo ""

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo -e "${RED}Error: Not in a git repository root directory${NC}"
    echo "Please run this script from the repository root"
    exit 1
fi

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo -e "${YELLOW}Warning: clang-format is not installed${NC}"
    echo ""
    echo "The pre-commit hook requires clang-format to be installed."
    echo "Please install it using one of the following commands:"
    echo ""
    echo -e "${BLUE}Ubuntu/Debian:${NC}"
    echo "  sudo apt-get install clang-format"
    echo ""
    echo -e "${BLUE}macOS:${NC}"
    echo "  brew install clang-format"
    echo ""
    echo -e "${BLUE}Fedora/RHEL:${NC}"
    echo "  sudo dnf install clang-tools-extra"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo -e "${GREEN}✓ clang-format is installed${NC}"
    clang-format --version
    echo ""
fi

# Create the pre-commit hook
HOOK_FILE=".git/hooks/pre-commit"

cat > "$HOOK_FILE" << 'EOF'
#!/bin/bash
#
# Git pre-commit hook to format C/C++ source files with clang-format
# This hook will automatically format staged C/C++ files before committing
#

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed."
    echo "Please install clang-format to use this pre-commit hook."
    echo "On Ubuntu/Debian: sudo apt-get install clang-format"
    echo "On macOS: brew install clang-format"
    exit 1
fi

# Get the list of staged C/C++ files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|h|cpp|hpp|cc|cxx)$')

# If no C/C++ files are staged, exit successfully
if [ -z "$STAGED_FILES" ]; then
    exit 0
fi

echo "Running clang-format on staged files..."

# Format each staged file
for FILE in $STAGED_FILES; do
    # Only format if the file exists (it might have been deleted)
    if [ -f "$FILE" ]; then
        echo "  Formatting: $FILE"
        clang-format -i "$FILE"
        # Re-stage the formatted file
        git add "$FILE"
    fi
done

echo "clang-format completed successfully."
exit 0
EOF

# Make the hook executable
chmod +x "$HOOK_FILE"

echo -e "${GREEN}✓ Pre-commit hook installed successfully${NC}"
echo ""
echo -e "${BLUE}The following hooks are now active:${NC}"
echo "  - pre-commit: Automatically formats C/C++ files with clang-format"
echo ""
echo -e "${BLUE}For more information, see:${NC}"
echo "  docs/GIT_HOOKS.md"
echo ""
echo -e "${GREEN}Setup complete!${NC}"

