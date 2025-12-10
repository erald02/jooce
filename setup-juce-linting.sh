#!/bin/bash
# setup-juce-linting.sh
# Automates the setup of linting for JUCE projects in Cursor IDE

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the project root (directory where script is run from)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/Builds/LinuxMakefile"
COMPILE_DB="${PROJECT_ROOT}/compile_commands.json"
CLANGD_CONFIG="${PROJECT_ROOT}/.clangd"

echo -e "${GREEN}Setting up JUCE linting for Cursor IDE...${NC}"

# Check if bear is installed
if ! command -v bear &> /dev/null; then
    echo -e "${RED}Error: bear is not installed.${NC}"
    echo "Install it with: sudo pacman -S bear"
    exit 1
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    exit 1
fi

# Step 1: Clean and regenerate compile_commands.json
echo -e "${YELLOW}Step 1: Regenerating compile_commands.json...${NC}"
cd "$BUILD_DIR"

# Remove old compile_commands.json if it exists
rm -f compile_commands.json

# Clean build to ensure fresh capture
echo "Cleaning build..."
make clean > /dev/null 2>&1 || true

# Capture compile commands with bear
echo "Capturing compile commands..."
if ! bear -- make -j$(nproc) > /dev/null 2>&1; then
    echo -e "${YELLOW}Warning: Build had errors, but continuing...${NC}"
fi

# Check if compile_commands.json was created
if [ ! -f "compile_commands.json" ]; then
    echo -e "${RED}Error: Failed to generate compile_commands.json${NC}"
    exit 1
fi

# Step 2: Move compile_commands.json to project root
echo -e "${YELLOW}Step 2: Moving compile_commands.json to project root...${NC}"
mv compile_commands.json "$COMPILE_DB"

# Step 3: Fix paths in compile_commands.json
echo -e "${YELLOW}Step 3: Fixing paths in compile_commands.json...${NC}"
cd "$PROJECT_ROOT"

python3 - "$COMPILE_DB" "$PROJECT_ROOT" "$BUILD_DIR" << 'PYTHON_SCRIPT'
import json
import os
import sys

compile_db_path = sys.argv[1]
project_root = sys.argv[2]
build_dir = sys.argv[3]

with open(compile_db_path, 'r') as f:
    data = json.load(f)

fixed_count = 0
for entry in data:
    # Fix file path - make it absolute
    if 'file' in entry:
        file_path = entry['file']
        if not os.path.isabs(file_path):
            # Try relative to build dir first
            abs_path = os.path.normpath(os.path.join(build_dir, file_path))
            if os.path.exists(abs_path):
                entry['file'] = os.path.relpath(abs_path, project_root)
            else:
                # Try relative to project root
                abs_path = os.path.normpath(os.path.join(project_root, file_path))
                if os.path.exists(abs_path):
                    entry['file'] = os.path.relpath(abs_path, project_root)
                else:
                    # Keep original but make it relative to project root
                    entry['file'] = file_path
        fixed_count += 1
    
    # Fix directory to project root
    entry['directory'] = project_root
    
    # Fix include paths in arguments
    if 'arguments' in entry:
        for i, arg in enumerate(entry['arguments']):
            if arg.startswith('-I'):
                include_path = arg[2:]
                # Handle relative paths
                if include_path.startswith('../../'):
                    abs_path = os.path.normpath(os.path.join(build_dir, include_path))
                    if os.path.exists(abs_path):
                        entry['arguments'][i] = '-I' + os.path.abspath(abs_path)
                    else:
                        # Try relative to project root
                        rel_path = include_path.replace('../../', '')
                        abs_path = os.path.normpath(os.path.join(project_root, rel_path))
                        if os.path.exists(abs_path):
                            entry['arguments'][i] = '-I' + os.path.abspath(abs_path)
                elif include_path.startswith('./'):
                    abs_path = os.path.normpath(os.path.join(project_root, include_path[2:]))
                    if os.path.exists(abs_path):
                        entry['arguments'][i] = '-I' + os.path.abspath(abs_path)
                elif not os.path.isabs(include_path):
                    # Try relative to build dir
                    abs_path = os.path.normpath(os.path.join(build_dir, include_path))
                    if os.path.exists(abs_path):
                        entry['arguments'][i] = '-I' + os.path.abspath(abs_path)
                    else:
                        # Try relative to project root
                        abs_path = os.path.normpath(os.path.join(project_root, include_path))
                        if os.path.exists(abs_path):
                            entry['arguments'][i] = '-I' + os.path.abspath(abs_path)

with open(compile_db_path, 'w') as f:
    json.dump(data, f, indent=2)

print(f"Fixed {fixed_count} entries in compile_commands.json")
PYTHON_SCRIPT

# Step 4: Extract JUCE defines from compile_commands.json
echo -e "${YELLOW}Step 4: Extracting JUCE configuration...${NC}"
JUCE_DEFINES=$(python3 - "$COMPILE_DB" << 'PYTHON_SCRIPT'
import json
import sys

with open(sys.argv[1], 'r') as f:
    data = json.load(f)

if not data:
    sys.exit(1)

# Get defines from first entry (they should be the same for all)
entry = data[0]
defines = []

if 'arguments' in entry:
    for arg in entry['arguments']:
        if arg.startswith('-D') and ('JUCE' in arg or 'LINUX' in arg or 'DEBUG' in arg):
            defines.append(arg[2:])  # Remove -D prefix

# Also get include paths
includes = []
for arg in entry.get('arguments', []):
    if arg.startswith('-I'):
        includes.append(arg[2:])

# Print defines and includes as JSON
import json
print(json.dumps({
    'defines': defines,
    'includes': includes,
    'std': next((arg.split('=')[1] for arg in entry.get('arguments', []) if arg.startswith('-std=')), 'c++20')
}))
PYTHON_SCRIPT
)

if [ -z "$JUCE_DEFINES" ]; then
    echo -e "${YELLOW}Warning: Could not extract JUCE defines, using defaults${NC}"
    JUCE_DEFINES='{"defines":["LINUX=1","DEBUG=1","_DEBUG=1","JUCE_STRICT_REFCOUNTEDPOINTER=1","JUCE_STANDALONE_APPLICATION=1"],"includes":[],"std":"c++20"}'
fi

# Step 5: Update .clangd configuration
echo -e "${YELLOW}Step 5: Updating .clangd configuration...${NC}"

python3 - "$PROJECT_ROOT" "$JUCE_DEFINES" << 'PYTHON_SCRIPT'
import json
import sys
import os

project_root = sys.argv[1]
juce_config = json.loads(sys.argv[2])

# Build the .clangd config
clangd_config = {
    'CompileFlags': {
        'Add': [],
        'Remove': ['-W*']
    },
    'Diagnostics': {
        'UnusedIncludes': 'None',
        'MissingIncludes': 'None'
    }
}

# Add C++ standard
clangd_config['CompileFlags']['Add'].append(f"-std={juce_config['std']}")

# Add include paths (use absolute paths)
juce_lib_path = os.path.join(project_root, 'JuceLibraryCode')
juce_modules_path = os.path.join(project_root, 'JUCE', 'modules')

if os.path.exists(juce_lib_path):
    clangd_config['CompileFlags']['Add'].append(f"-I{os.path.abspath(juce_lib_path)}")
if os.path.exists(juce_modules_path):
    clangd_config['CompileFlags']['Add'].append(f"-I{os.path.abspath(juce_modules_path)}")

# Add all defines
for define in juce_config['defines']:
    clangd_config['CompileFlags']['Add'].append(f"-D{define}")

# Write .clangd file
clangd_path = os.path.join(project_root, '.clangd')
with open(clangd_path, 'w') as f:
    f.write('CompileFlags:\n')
    f.write('  Add:\n')
    for flag in clangd_config['CompileFlags']['Add']:
        f.write(f'    - {flag}\n')
    f.write('  Remove:\n')
    for flag in clangd_config['CompileFlags']['Remove']:
        f.write(f'    - {flag}\n')
    f.write('Diagnostics:\n')
    f.write(f"  UnusedIncludes: {clangd_config['Diagnostics']['UnusedIncludes']}\n")
    f.write(f"  MissingIncludes: {clangd_config['Diagnostics']['MissingIncludes']}\n")

print(f"Updated .clangd with {len(clangd_config['CompileFlags']['Add'])} flags")
PYTHON_SCRIPT

echo -e "${GREEN}✓ Linting setup complete!${NC}"
echo ""
echo "Next steps:"
echo "1. Restart Cursor IDE (or reload window: Ctrl+Shift+P → 'Developer: Reload Window')"
echo "2. Open a .cpp or .h file to verify linting is working"
echo "3. Check clangd output: Ctrl+Shift+P → 'Output' → select 'clangd'"
echo ""
echo "Files created/updated:"
echo "  - ${COMPILE_DB}"
echo "  - ${CLANGD_CONFIG}"