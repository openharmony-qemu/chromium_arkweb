#!/bin/bash
# Copyright (c) 2026 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# set -ex

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
while [ "$PROJECT_ROOT" != "/" ] && [ ! -f "$PROJECT_ROOT/include/v8-version.h" ]; do
    PROJECT_ROOT="$(dirname "$PROJECT_ROOT")"
done
if [ ! -f "$PROJECT_ROOT/include/v8-version.h" ]; then
    echo "Failed to locate V8 project root from $SCRIPT_DIR" >&2
    exit 1
fi

# Detect the current operating system
OS=$(uname -s)

# Get V8 commit ID
COMMIT_ID=$(cd "$PROJECT_ROOT" && git rev-parse HEAD)

# Get V8 version information
V8_MAJOR_VERSION=$(grep "V8_MAJOR_VERSION" "$PROJECT_ROOT/include/v8-version.h" | cut -d ' ' -f 3)
V8_MINOR_VERSION=$(grep "V8_MINOR_VERSION" "$PROJECT_ROOT/include/v8-version.h" | cut -d ' ' -f 3)
V8_BUILD_NUMBER=$(grep "V8_BUILD_NUMBER" "$PROJECT_ROOT/include/v8-version.h" | cut -d ' ' -f 3)
V8_PATCH_LEVEL=$(grep "V8_PATCH_LEVEL" "$PROJECT_ROOT/include/v8-version.h" | cut -d ' ' -f 3)
V8_INNER_VERSION=$(grep "V8_INNER_VERSION" "$PROJECT_ROOT/include/v8-version.h" | cut -d ' ' -f 3 || echo "")

# Create version info file
cat > "$SCRIPT_DIR/version_info.txt" << EOF
V8 Commit ID: $COMMIT_ID
V8 Version: $V8_MAJOR_VERSION.$V8_MINOR_VERSION.$V8_BUILD_NUMBER.$V8_PATCH_LEVEL${V8_INNER_VERSION:+.$V8_INNER_VERSION}
V8_MAJOR_VERSION: $V8_MAJOR_VERSION
V8_MINOR_VERSION: $V8_MINOR_VERSION
V8_BUILD_NUMBER: $V8_BUILD_NUMBER
V8_PATCH_LEVEL: $V8_PATCH_LEVEL
V8_INNER_VERSION: $V8_INNER_VERSION
EOF

if [ "$OS" = "Linux" ]; then
    # On Linux, build Windows and Linux targets
    # Generate build configurations with gn gen
    mkdir -p "$PROJECT_ROOT/out/win64.sandbox"
    cp "$SCRIPT_DIR/windows.x64.sandbox.args.gn" "$PROJECT_ROOT/out/win64.sandbox/args.gn"
    gn gen "$PROJECT_ROOT/out/win64.sandbox"

    mkdir -p "$PROJECT_ROOT/out/win64.nosandbox"
    cp "$SCRIPT_DIR/windows.x64.nosandbox.args.gn" "$PROJECT_ROOT/out/win64.nosandbox/args.gn"
    gn gen "$PROJECT_ROOT/out/win64.nosandbox"

    mkdir -p "$PROJECT_ROOT/out/x64.sandbox"
    cp "$SCRIPT_DIR/linux.x64.sandbox.args.gn" "$PROJECT_ROOT/out/x64.sandbox/args.gn"
    gn gen "$PROJECT_ROOT/out/x64.sandbox"

    mkdir -p "$PROJECT_ROOT/out/x64.nosandbox"
    cp "$SCRIPT_DIR/linux.x64.nosandbox.args.gn" "$PROJECT_ROOT/out/x64.nosandbox/args.gn"
    gn gen "$PROJECT_ROOT/out/x64.nosandbox"

    ninja -C "$PROJECT_ROOT/out/win64.sandbox" js_rawheap_translator
    ninja -C "$PROJECT_ROOT/out/win64.nosandbox" js_rawheap_translator
    ninja -C "$PROJECT_ROOT/out/x64.sandbox" js_rawheap_translator
    ninja -C "$PROJECT_ROOT/out/x64.nosandbox" js_rawheap_translator

    # Copy Windows binaries
    mkdir -p "$SCRIPT_DIR/windows"
    cp "$PROJECT_ROOT/out/win64.sandbox/translate.exe" "$SCRIPT_DIR/windows/t1"
    cp "$PROJECT_ROOT/out/win64.nosandbox/translate.exe" "$SCRIPT_DIR/windows/t2"
    cp "$PROJECT_ROOT/out/win64.sandbox/js_rawheap_translator.exe" "$SCRIPT_DIR/windows/"

    # Copy Linux binaries
    mkdir -p "$SCRIPT_DIR/linux"
    cp "$PROJECT_ROOT/out/x64.sandbox/translate" "$SCRIPT_DIR/linux/t1"
    cp "$PROJECT_ROOT/out/x64.nosandbox/translate" "$SCRIPT_DIR/linux/t2"
    cp "$PROJECT_ROOT/out/x64.sandbox/js_rawheap_translator" "$SCRIPT_DIR/linux/"

    # Strip Linux binaries
    strip "$SCRIPT_DIR/linux/t1"
    strip "$SCRIPT_DIR/linux/t2"
    strip "$SCRIPT_DIR/linux/js_rawheap_translator"

    # Archive stripped binaries with version info
    cd "$SCRIPT_DIR"
    cp version_info.txt linux/
    tar -czf "js_rawheap_translator_linux.tar.gz" linux/
    cp version_info.txt windows/
    tar -czf "js_rawheap_translator_windows.tar.gz" windows/
    cd - > /dev/null

elif [ "$OS" = "Darwin" ]; then
    # On macOS, only build local (arm64) targets
    # Generate build configurations with gn gen
    mkdir -p "$PROJECT_ROOT/out/arm64.sandbox"
    cp "$SCRIPT_DIR/darwin.arm64.sandbox.args.gn" "$PROJECT_ROOT/out/arm64.sandbox/args.gn"
    gn gen "$PROJECT_ROOT/out/arm64.sandbox"

    mkdir -p "$PROJECT_ROOT/out/arm64.nosandbox"
    cp "$SCRIPT_DIR/darwin.arm64.nosandbox.args.gn" "$PROJECT_ROOT/out/arm64.nosandbox/args.gn"
    gn gen "$PROJECT_ROOT/out/arm64.nosandbox"

    ninja -C "$PROJECT_ROOT/out/arm64.sandbox" js_rawheap_translator
    ninja -C "$PROJECT_ROOT/out/arm64.nosandbox" js_rawheap_translator

    # Copy macOS binaries
    mkdir -p "$SCRIPT_DIR/darwin"
    cp "$PROJECT_ROOT/out/arm64.sandbox/translate" "$SCRIPT_DIR/darwin/t1"
    cp "$PROJECT_ROOT/out/arm64.nosandbox/translate" "$SCRIPT_DIR/darwin/t2"
    cp "$PROJECT_ROOT/out/arm64.sandbox/js_rawheap_translator" "$SCRIPT_DIR/darwin/"

    # Strip macOS binaries
    strip "$SCRIPT_DIR/darwin/t1"
    strip "$SCRIPT_DIR/darwin/t2"
    strip "$SCRIPT_DIR/darwin/js_rawheap_translator"

    # Archive stripped binaries with version info
    cd "$SCRIPT_DIR"
    cp version_info.txt darwin/
    tar -czf "js_rawheap_translator_darwin.tar.gz" darwin/
    cd - > /dev/null
else
    echo "Unsupported operating system: $OS"
    exit 1
fi

# Clean up version info file
rm "$SCRIPT_DIR/version_info.txt"
