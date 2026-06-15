#!/usr/bin/env bash
# One-shot release script: builds every plugin (AU + VST3, Release) and
# produces a single macOS .pkg installer that lets the end-user choose which
# plugins and which formats to install.
#
# Usage:
#   ./make_release.sh                                 # version 1.0.0, all plugins, AU+VST3
#   ./make_release.sh 2.1.0                           # explicit version
#   ./make_release.sh 2.1.0 --plugins AF-1,Lupo       # subset only
#   ./make_release.sh 2.1.0 --plugins Pike            # single-plugin installer (Pike-2.1.0.pkg)
#   ./make_release.sh 2.1.0 --each                    # one installer per plugin
#   ./make_release.sh 2.1.0 --name MyBundle           # override installer name/title
#   ./make_release.sh 2.1.0 --formats AU              # AU or VST3 only
#   ./make_release.sh 2.1.0 --clean                   # clean each Xcode project first
#   ./make_release.sh 2.1.0 --skip-build              # reuse existing artifacts
#
# This script is just a thin wrapper around build_all.sh + create_installer.sh.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

VERSION="1.0.0"
SKIP_BUILD=0
BUILD_ARGS=()
INSTALLER_ARGS=()

if [[ $# -gt 0 && "$1" != --* ]]; then
    VERSION="$1"; shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1; shift ;;
        --clean)      BUILD_ARGS+=(--clean); shift ;;
        --plugins)    BUILD_ARGS+=(--plugins "$2"); INSTALLER_ARGS+=(--plugins "$2"); shift 2 ;;
        --formats)    BUILD_ARGS+=(--formats "$2"); INSTALLER_ARGS+=(--formats "$2"); shift 2 ;;
        --name)       INSTALLER_ARGS+=(--name "$2"); shift 2 ;;
        --each)       INSTALLER_ARGS+=(--each); shift ;;
        --jobs)       BUILD_ARGS+=(--jobs "$2"); shift 2 ;;
        -h|--help)    sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

if (( SKIP_BUILD )); then
    echo "==> --skip-build set, skipping compilation step"
else
    echo "════════════════════════════════════════════════════════════════════"
    echo "  Phase 1/2 — Building plugins (Release)"
    echo "════════════════════════════════════════════════════════════════════"
    "$SCRIPT_DIR/build_all.sh" ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"}
fi

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "  Phase 2/2 — Creating installer ($VERSION)"
echo "════════════════════════════════════════════════════════════════════"
"$SCRIPT_DIR/create_installer.sh" "$VERSION" ${INSTALLER_ARGS[@]+"${INSTALLER_ARGS[@]}"}
