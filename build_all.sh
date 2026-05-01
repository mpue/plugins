#!/usr/bin/env bash
# Builds all JUCE plugins in this repository as AU and/or VST3 in Release
# configuration. Works on macOS with Xcode (>= command line tools that ship
# with a full Xcode install).
#
# Usage:
#   ./build_all.sh                       # build every plugin as AU + VST3
#   ./build_all.sh --clean               # clean each project before building
#   ./build_all.sh --plugins AF-1,Lupo   # only these plugins
#   ./build_all.sh --formats AU          # only AU (or VST3, or AU,VST3)
#   ./build_all.sh --jobs 4              # parallel xcodebuild jobs (-jobs flag)
#
# Exit codes: 0 = success, non-zero = at least one build failed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── All plugins in the repo (subdir name == .jucer name) ─────────────────────
ALL_PLUGINS=(AF-1 BC-1 CH-1 CP-1 DL-1 EQ8 LT-1 Lupo PS-1 RV-1 ST-1 SW-1 TS-1)
ALL_FORMATS=(AU VST3)

# ── Args ─────────────────────────────────────────────────────────────────────
PLUGINS=()
FORMATS=()
CLEAN=0
JOBS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)        CLEAN=1; shift ;;
        --plugins)      IFS=',' read -r -a PLUGINS <<< "$2"; shift 2 ;;
        --formats)      IFS=',' read -r -a FORMATS <<< "$2"; shift 2 ;;
        --jobs)         JOBS="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

[[ ${#PLUGINS[@]} -eq 0 ]] && PLUGINS=("${ALL_PLUGINS[@]}")
[[ ${#FORMATS[@]} -eq 0 ]] && FORMATS=("${ALL_FORMATS[@]}")

# ── Locate Xcode ─────────────────────────────────────────────────────────────
# JUCE projects need full Xcode (xcodebuild), not just command line tools.
if ! xcodebuild -version >/dev/null 2>&1; then
    if [[ -d /Applications/Xcode.app ]]; then
        export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
        echo "==> Using DEVELOPER_DIR=$DEVELOPER_DIR"
    else
        echo "ERROR: xcodebuild not available and /Applications/Xcode.app missing." >&2
        echo "       Install Xcode and run: sudo xcode-select -s /Applications/Xcode.app" >&2
        exit 1
    fi
fi

# ── Validate selections ──────────────────────────────────────────────────────
for p in "${PLUGINS[@]}"; do
    if [[ ! -d "$SCRIPT_DIR/$p/Builds/MacOSX" ]]; then
        echo "ERROR: plugin '$p' not found (no $p/Builds/MacOSX)." >&2
        exit 1
    fi
done
for f in "${FORMATS[@]}"; do
    case "$f" in AU|VST3) ;; *)
        echo "ERROR: unknown format '$f' (allowed: AU, VST3)." >&2; exit 1 ;;
    esac
done

# ── Build loop ───────────────────────────────────────────────────────────────
FAILED=()
SUCCEEDED=()
START_TIME=$(date +%s)

build_one() {
    local plugin="$1" format="$2"
    local proj_dir="$SCRIPT_DIR/$plugin/Builds/MacOSX"
    local proj="$proj_dir/$plugin.xcodeproj"
    local scheme="$plugin - $format"
    local log="$proj_dir/build/${plugin}-${format}.log"

    mkdir -p "$proj_dir/build"
    echo "==> [$plugin] building scheme '$scheme'  (log: $log)"

    local xcb_args=(
        -project "$proj"
        -scheme  "$scheme"
        -configuration Release
        -quiet
    )
    [[ -n "$JOBS" ]] && xcb_args+=(-jobs "$JOBS")

    if (( CLEAN )); then
        xcodebuild "${xcb_args[@]}" clean >"$log" 2>&1 || true
    fi
    if ! xcodebuild "${xcb_args[@]}" build >"$log" 2>&1; then
        echo "    ✗ FAILED — see $log"
        FAILED+=("$plugin/$format")
        return 1
    fi

    # Verify the artifact landed where we expect.
    local artifact_ext
    case "$format" in
        AU)   artifact_ext="component" ;;
        VST3) artifact_ext="vst3" ;;
    esac
    local artifact="$proj_dir/build/Release/$plugin.$artifact_ext"
    if [[ ! -e "$artifact" ]]; then
        echo "    ✗ artifact missing: $artifact"
        FAILED+=("$plugin/$format")
        return 1
    fi
    echo "    ✓ $artifact"
    SUCCEEDED+=("$plugin/$format")
    return 0
}

for plugin in "${PLUGINS[@]}"; do
    for format in "${FORMATS[@]}"; do
        build_one "$plugin" "$format" || true
    done
done

# ── Report ───────────────────────────────────────────────────────────────────
ELAPSED=$(( $(date +%s) - START_TIME ))
echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "  Build summary  (${ELAPSED}s)"
echo "════════════════════════════════════════════════════════════════════"
echo "  Succeeded: ${#SUCCEEDED[@]}"
for s in "${SUCCEEDED[@]}"; do echo "    ✓ $s"; done
echo "  Failed:    ${#FAILED[@]}"
for f in "${FAILED[@]}"; do echo "    ✗ $f"; done
echo "════════════════════════════════════════════════════════════════════"

[[ ${#FAILED[@]} -eq 0 ]] || exit 1
