#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
PLUGIN_NAME="Drawdio.vst3"
RECONFIGURE=0
NO_INSTALL=0
PARALLEL_JOBS=""
INSTALL_DIR=""
INSTALL_DIR_EXPLICIT=0

usage() {
    cat <<'EOF'
Usage: ./updater.sh [options]

Build Release VST3 + Standalone (+ AU on macOS) and install the VST3 bundle.

Options:
  --help                  Show this help text.
  --reconfigure           Re-run CMake configuration without deleting build files.
  --parallel N            Pass an explicit parallel job count to CMake.
  --install-dir PATH      Install into PATH instead of the platform default.
  --no-install            Build and validate the plugin without installing it.

The CMAKE_BUILD_PARALLEL_LEVEL environment variable controls parallelism when
--parallel is not supplied. The script builds VST3 + Standalone on all
platforms and AU on macOS; it installs VST3 only.
EOF
}

fail() {
    printf 'Error: %s\n' "$1" >&2
    exit 1
}

normalize_windows_path() {
    local value="$1"
    if command -v cygpath >/dev/null 2>&1 && [[ "$value" == *:* || "$value" == \\* ]]; then
        cygpath -u "$value"
    else
        printf '%s\n' "$value"
    fi
}

while (($# > 0)); do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --reconfigure)
            RECONFIGURE=1
            shift
            ;;
        --no-install)
            NO_INSTALL=1
            shift
            ;;
        --parallel|--jobs)
            (($# >= 2)) || fail "$1 requires a positive integer"
            [[ "$2" =~ ^[1-9][0-9]*$ ]] || fail "$1 requires a positive integer"
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --install-dir)
            (($# >= 2)) || fail "--install-dir requires a path"
            INSTALL_DIR="$2"
            INSTALL_DIR_EXPLICIT=1
            shift 2
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

UNAME="$(uname -s 2>/dev/null || true)"
case "$UNAME" in
    Darwin*)
        PLATFORM="macOS"
        DEFAULT_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        PLATFORM="Windows"
        if [[ -n "${ProgramW6432:-}" ]]; then
            WINDOWS_PROGRAM_FILES="$(normalize_windows_path "$ProgramW6432")"
        elif [[ -n "${PROGRAMW6432:-}" ]]; then
            WINDOWS_PROGRAM_FILES="$(normalize_windows_path "$PROGRAMW6432")"
        elif [[ -n "${PROGRAMFILES:-}" ]]; then
            WINDOWS_PROGRAM_FILES="$(normalize_windows_path "$PROGRAMFILES")"
        else
            WINDOWS_PROGRAM_FILES="/c/Program Files"
        fi
        DEFAULT_INSTALL_DIR="$WINDOWS_PROGRAM_FILES/Common Files/VST3"
        ;;
    Linux*)
        PLATFORM="Linux"
        DEFAULT_INSTALL_DIR="${HOME:-/tmp}/.vst3"
        ;;
    *)
        fail "unsupported shell environment: ${UNAME:-unknown}. Use macOS, Linux, or Windows Git Bash/MSYS2."
        ;;
esac

if [[ "$INSTALL_DIR_EXPLICIT" == 1 && "$PLATFORM" == "Windows" ]]; then
    INSTALL_DIR="$(normalize_windows_path "$INSTALL_DIR")"
elif [[ "$INSTALL_DIR_EXPLICIT" == 0 ]]; then
    INSTALL_DIR="$DEFAULT_INSTALL_DIR"
fi

if [[ "$RECONFIGURE" == 1 || ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "Configuring Release build..."
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

echo "Building Release VST3 + Standalone (+ AU on macOS)..."
if [[ "$PLATFORM" == "macOS" ]]; then
    BUILD_ARGS=(--build "$BUILD_DIR" --config Release --target Drawdio_VST3 --target Drawdio_Standalone --target Drawdio_AU --parallel)
else
    BUILD_ARGS=(--build "$BUILD_DIR" --config Release --target Drawdio_VST3 --target Drawdio_Standalone --parallel)
fi
if [[ -n "$PARALLEL_JOBS" ]]; then
    BUILD_ARGS+=("$PARALLEL_JOBS")
fi
cmake "${BUILD_ARGS[@]}"

SOURCE_PLUGIN="$BUILD_DIR/Drawdio_artefacts/Release/VST3/$PLUGIN_NAME"
if [[ ! -d "$SOURCE_PLUGIN" || ! -d "$SOURCE_PLUGIN/Contents" ]]; then
    fail "Release VST3 bundle was not produced at $SOURCE_PLUGIN"
fi

STANDALONE_DIR="$BUILD_DIR/Drawdio_artefacts/Release/Standalone"
AU_BUNDLE="$BUILD_DIR/Drawdio_artefacts/Release/AU/Drawdio.component"
if [[ ! -d "$STANDALONE_DIR" ]]; then
    fail "Release Standalone was not produced at $STANDALONE_DIR"
fi
if [[ "$PLATFORM" == "macOS" && ! -d "$AU_BUNDLE" ]]; then
    echo "Warning: AU bundle not produced at $AU_BUNDLE"
fi

if [[ "$NO_INSTALL" == 1 ]]; then
    echo "Validated: $SOURCE_PLUGIN"
    echo "Validated: $STANDALONE_DIR"
    if [[ "$PLATFORM" == "macOS" ]]; then
        [[ -d "$AU_BUNDLE" ]] && echo "Validated: $AU_BUNDLE" || echo "AU not present (skip)"
    fi
    exit 0
fi

validate_bundle() {
    local bundle="$1"
    [[ -d "$bundle" && -d "$bundle/Contents" ]] || return 1
    [[ -d "$bundle/Contents/Resources" || -d "$bundle/Contents/MacOS" || -d "$bundle/Contents/x86_64-win" || -d "$bundle/Contents/x86_64-linux" ]]
}

install_to_directory() {
    local target_dir="$1"
    local destination="$target_dir/$PLUGIN_NAME"
    local tmp_root="${TMPDIR:-${TMP:-/tmp}}"
    local staging=""
    local backup=""
    local staged_bundle=""

    if ! mkdir -p "$target_dir" 2>/dev/null; then
        return 1
    fi

    rm -rf "$target_dir"/.Drawdio.vst3.* 2>/dev/null || true

    if ! validate_bundle "$SOURCE_PLUGIN"; then
        return 1
    fi

    staging="$(mktemp -d "$tmp_root/Drawdio.vst3.staging.$$.XXXXXX" 2>/dev/null)" || return 1
    backup="$tmp_root/Drawdio.vst3.previous.$$"
    staged_bundle="$staging/$PLUGIN_NAME"
    rm -rf "$backup" 2>/dev/null || true

    if ! cp -R "$SOURCE_PLUGIN" "$staged_bundle" 2>/dev/null; then
        rm -rf "$staging" "$backup" 2>/dev/null || true
        return 1
    fi
    if ! validate_bundle "$staged_bundle"; then
        rm -rf "$staging" "$backup" 2>/dev/null || true
        return 1
    fi

    if [[ -e "$destination" ]]; then
        if ! mv "$destination" "$backup" 2>/dev/null; then
            rm -rf "$staging" "$backup" 2>/dev/null || true
            return 1
        fi
    fi

    if ! mv "$staged_bundle" "$destination" 2>/dev/null; then
        if [[ -e "$backup" ]]; then
            mv "$backup" "$destination" 2>/dev/null || true
        fi
        rm -rf "$staging" 2>/dev/null || true
        rm -rf "$backup" 2>/dev/null || true
        return 1
    fi

    rm -rf "$staging" 2>/dev/null || true
    rm -rf "$backup" 2>/dev/null || true
    return 0
}

echo "Installing to $INSTALL_DIR..."
if ! install_to_directory "$INSTALL_DIR"; then
    if [[ "$PLATFORM" == "Windows" && "$INSTALL_DIR_EXPLICIT" == 0 ]]; then
        if [[ -n "${LOCALAPPDATA:-}" ]]; then
            WINDOWS_USER_DIR="$(normalize_windows_path "$LOCALAPPDATA")/Programs/Common/VST3"
        else
            WINDOWS_USER_DIR="$HOME/AppData/Local/Programs/Common/VST3"
        fi
        if [[ "$WINDOWS_USER_DIR" != "$INSTALL_DIR" ]]; then
            echo "System installation was not writable; trying per-user VST3 directory..."
            INSTALL_DIR="$WINDOWS_USER_DIR"
            if install_to_directory "$INSTALL_DIR"; then
                echo "Installed to $INSTALL_DIR/$PLUGIN_NAME"
                echo "Standalone: $STANDALONE_DIR"
                if [[ "$PLATFORM" == "macOS" && -d "$AU_BUNDLE" ]]; then
                    echo "AU: $AU_BUNDLE"
                fi
                echo "Restart or rescan the DAW if it does not discover the updated plugin."
                exit 0
            fi
        fi
    fi

    if [[ "$PLATFORM" == "Windows" ]]; then
        fail "could not install the VST3 bundle. Close any DAW using Drawdio and run Git Bash as Administrator, or pass --install-dir to a writable directory"
    fi
    fail "could not install the VST3 bundle into $INSTALL_DIR"
fi

echo "Installed to $INSTALL_DIR/$PLUGIN_NAME"
echo "Standalone: $STANDALONE_DIR"
if [[ "$PLATFORM" == "macOS" && -d "$AU_BUNDLE" ]]; then
    echo "AU: $AU_BUNDLE"
fi
echo "Restart or rescan the DAW if it does not discover the updated plugin."
