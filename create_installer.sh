#!/usr/bin/env bash
# Builds a macOS .pkg installer that bundles every plugin in this repo as a
# component package. The resulting installer presents a customisable choice
# tree: top-level groups by format (AU / VST3), with one toggle per plugin
# inside each group. The user can therefore pick:
#   • which formats to install (top-level)
#   • which individual plugins to install (per format)
#
# Usage:
#   ./create_installer.sh                             # version 1.0.0
#   ./create_installer.sh 2.1.0                       # explicit version
#   ./create_installer.sh 2.1.0 --plugins AF-1,Lupo   # subset only
#   ./create_installer.sh 2.1.0 --formats VST3        # AU or VST3 only
#
# Requires the plugins to have been built into
#   <plugin>/Builds/MacOSX/build/Release/<plugin>.{component,vst3}
# (use ./build_all.sh first, or run ./make_release.sh which does both).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ALL_PLUGINS=(AF-1 BC-1 BS-1 CD-1 CH-1 CP-1 CRV-1 DL-1 EQ8 FP-1 GS-1 HH-1 KM-1 LT-1 Lupo PM-1 PS-1 RV-1 SA-1 SN-1 ST-1 SW-1 TR-1 TS-1)
ALL_FORMATS=(AU VST3)

# ── Args ─────────────────────────────────────────────────────────────────────
VERSION="1.0.0"
PLUGINS=()
FORMATS=()

# First positional arg (if it doesn't start with --) is the version.
if [[ $# -gt 0 && "$1" != --* ]]; then
    VERSION="$1"; shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --plugins)  IFS=',' read -r -a PLUGINS <<< "$2"; shift 2 ;;
        --formats)  IFS=',' read -r -a FORMATS <<< "$2"; shift 2 ;;
        -h|--help)  sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

[[ ${#PLUGINS[@]} -eq 0 ]] && PLUGINS=("${ALL_PLUGINS[@]}")
[[ ${#FORMATS[@]} -eq 0 ]] && FORMATS=("${ALL_FORMATS[@]}")

INSTALLER_DIR="$SCRIPT_DIR/Installer"
STAGING_DIR="$INSTALLER_DIR/staging"
PKG_DIR="$INSTALLER_DIR/packages"
RESOURCES_DIR="$INSTALLER_DIR/resources"
OUTPUT_DIR="$INSTALLER_DIR/output"
DIST_XML="$INSTALLER_DIR/distribution.xml"

INSTALLER_NAME="Pueski-Plugins"
TOP_TITLE="Pueski Plugin Suite"

# ── Helpers ──────────────────────────────────────────────────────────────────
plugin_manufacturer() {
    # Pull manufacturer from the .jucer file, falling back to companyName.
    local plugin="$1"
    local jucer="$SCRIPT_DIR/$plugin/$plugin.jucer"
    local m
    m=$(grep -o 'pluginManufacturer="[^"]*"' "$jucer" 2>/dev/null \
        | head -1 | sed 's/^pluginManufacturer="//; s/"$//')
    if [[ -z "$m" ]]; then
        m=$(grep -o 'companyName="[^"]*"' "$jucer" 2>/dev/null \
            | head -1 | sed 's/^companyName="//; s/"$//')
    fi
    [[ -z "$m" ]] && m="Pueski"
    echo "$m"
}

bundle_id() {
    # com.<manufacturer>.<plugin>.<format>  — manufacturer collapsed to alnum.
    local plugin="$1" format="$2"
    local manuf
    manuf=$(plugin_manufacturer "$plugin")
    manuf=$(echo "$manuf" | tr -cd '[:alnum:]')
    [[ -z "$manuf" ]] && manuf="Pueski"
    echo "com.$manuf.$plugin.$(echo "$format" | tr '[:upper:]' '[:lower:]')"
}

artifact_for() {
    local plugin="$1" format="$2"
    local rel="$SCRIPT_DIR/$plugin/Builds/MacOSX/build/Release"
    case "$format" in
        AU)   echo "$rel/$plugin.component" ;;
        VST3) echo "$rel/$plugin.vst3" ;;
    esac
}

install_subdir_for() {
    # macOS install location for each plugin format.
    case "$1" in
        AU)   echo "Library/Audio/Plug-Ins/Components" ;;
        VST3) echo "Library/Audio/Plug-Ins/VST3" ;;
    esac
}

xml_escape() {
    local s="$1"
    s="${s//&/&amp;}"; s="${s//</&lt;}"; s="${s//>/&gt;}"; s="${s//\"/&quot;}"
    echo "$s"
}

# ── Sanity: verify that every requested plugin/format artifact exists ────────
echo "==> Verifying plugin artifacts"
MISSING=()
for plugin in "${PLUGINS[@]}"; do
    for format in "${FORMATS[@]}"; do
        art=$(artifact_for "$plugin" "$format")
        if [[ ! -e "$art" ]]; then
            MISSING+=("$plugin/$format → $art")
        fi
    done
done
if [[ ${#MISSING[@]} -gt 0 ]]; then
    echo "ERROR: the following build artifacts are missing:" >&2
    printf '  - %s\n' "${MISSING[@]}" >&2
    echo "" >&2
    echo "Run ./build_all.sh first (or use ./make_release.sh)." >&2
    exit 1
fi

# ── Reset staging ────────────────────────────────────────────────────────────
rm -rf "$STAGING_DIR" "$PKG_DIR"
mkdir -p "$STAGING_DIR" "$PKG_DIR" "$OUTPUT_DIR" "$RESOURCES_DIR"

# ── Build one component .pkg per (plugin, format) ────────────────────────────
declare -a PKG_REFS=()              # all <pkg-ref> identifiers
declare -a CHOICE_LINES_AU=()       # outline children for AU group
declare -a CHOICE_LINES_VST3=()     # outline children for VST3 group
declare -a CHOICE_BLOCKS=()         # individual <choice> blocks
declare -a PKG_REF_BLOCKS=()        # <pkg-ref ...>file.pkg</pkg-ref> blocks

for plugin in "${PLUGINS[@]}"; do
    for format in "${FORMATS[@]}"; do
        art=$(artifact_for "$plugin" "$format")
        bid=$(bundle_id "$plugin" "$format")
        sub=$(install_subdir_for "$format")
        pkg_filename="${plugin}-${format}.pkg"
        choice_id="${plugin//-/_}_${format}"

        echo "--> Staging $plugin ($format)"
        stage_root="$STAGING_DIR/$plugin-$format"
        mkdir -p "$stage_root/$sub"
        # cp -R copies the bundle as-is, preserving the .component / .vst3 dir.
        cp -R "$art" "$stage_root/$sub/"

        pkgbuild \
            --root         "$stage_root" \
            --identifier   "$bid" \
            --version      "$VERSION" \
            --install-location "/" \
            "$PKG_DIR/$pkg_filename" >/dev/null

        echo "    -> $PKG_DIR/$pkg_filename"

        # Description shown in the installer customise pane.
        desc="Installs the $plugin $format plugin to /$sub."

        CHOICE_BLOCKS+=("$(cat <<XML
    <choice id="$choice_id"
            title="$plugin"
            description="$(xml_escape "$desc")"
            start_selected="true"
            start_enabled="true"
            start_visible="true">
        <pkg-ref id="$bid"/>
    </choice>
XML
)")

        PKG_REF_BLOCKS+=("    <pkg-ref id=\"$bid\" version=\"$VERSION\" onConclusion=\"none\">$pkg_filename</pkg-ref>")

        case "$format" in
            AU)   CHOICE_LINES_AU+=("        <line choice=\"$choice_id\"/>") ;;
            VST3) CHOICE_LINES_VST3+=("        <line choice=\"$choice_id\"/>") ;;
        esac
        PKG_REFS+=("$bid")
    done
done

# ── Format-group "parent" choices (toggling these toggles every child) ───────
GROUP_CHOICES=""
OUTLINE=""
have_au=0; have_vst3=0
for f in "${FORMATS[@]}"; do
    [[ "$f" == "AU" ]]   && have_au=1
    [[ "$f" == "VST3" ]] && have_vst3=1
done

if (( have_au )); then
    GROUP_CHOICES+=$(cat <<XML

    <choice id="group_AU"
            title="Audio Unit (AU)"
            description="All Audio Unit (.component) plugins. Installed to /Library/Audio/Plug-Ins/Components."
            start_selected="true"
            start_enabled="true"
            start_visible="true"/>
XML
)
    OUTLINE+="    <line choice=\"group_AU\">"$'\n'
    for l in "${CHOICE_LINES_AU[@]}"; do OUTLINE+="$l"$'\n'; done
    OUTLINE+="    </line>"$'\n'
fi
if (( have_vst3 )); then
    GROUP_CHOICES+=$(cat <<XML

    <choice id="group_VST3"
            title="VST3"
            description="All VST3 (.vst3) plugins. Installed to /Library/Audio/Plug-Ins/VST3."
            start_selected="true"
            start_enabled="true"
            start_visible="true"/>
XML
)
    OUTLINE+="    <line choice=\"group_VST3\">"$'\n'
    for l in "${CHOICE_LINES_VST3[@]}"; do OUTLINE+="$l"$'\n'; done
    OUTLINE+="    </line>"$'\n'
fi

# ── distribution.xml ─────────────────────────────────────────────────────────
{
    echo '<?xml version="1.0" encoding="utf-8"?>'
    echo '<installer-gui-script minSpecVersion="2">'
    echo "    <title>$TOP_TITLE $VERSION</title>"
    echo '    <welcome  file="welcome.html"  mime-type="text/html" />'
    echo '    <license  file="license.html"  mime-type="text/html" />'
    echo '    <readme   file="readme.html"   mime-type="text/html" />'
    echo '    <options customize="allow" require-scripts="false" rootVolumeOnly="false"/>'
    echo '    <choices-outline>'
    printf '%s' "$OUTLINE"
    echo '    </choices-outline>'
    printf '%s\n' "$GROUP_CHOICES"
    for c in "${CHOICE_BLOCKS[@]}"; do
        printf '%s\n' "$c"
    done
    for r in "${PKG_REF_BLOCKS[@]}"; do
        printf '%s\n' "$r"
    done
    echo '</installer-gui-script>'
} > "$DIST_XML"

# ── Default HTML resources (don't overwrite if user supplied real ones) ──────
write_if_missing() {
    local path="$1" content="$2"
    [[ -f "$path" ]] && return 0
    printf '%s' "$content" > "$path"
}

YEAR=$(date +%Y)
PLUGIN_LIST_HTML=""
for p in "${PLUGINS[@]}"; do
    PLUGIN_LIST_HTML+="<li>$p</li>"
done

write_if_missing "$RESOURCES_DIR/welcome.html" "<html><body>
<h2>$TOP_TITLE $VERSION</h2>
<p>This installer bundles the following plugins by Matthias Pueski:</p>
<ul>$PLUGIN_LIST_HTML</ul>
<p>On the next screens you can pick the formats (Audio Unit / VST3) and the
individual plugins you wish to install.</p>
</body></html>"

write_if_missing "$RESOURCES_DIR/license.html" "<html><body>
<h2>License</h2>
<p>Copyright &copy; $YEAR Matthias Pueski. All rights reserved.</p>
<p>This software is provided &quot;as is&quot;, without warranty of any kind.</p>
</body></html>"

write_if_missing "$RESOURCES_DIR/readme.html" "<html><body>
<h2>$TOP_TITLE $VERSION &mdash; Read Me</h2>
<p>After installation, restart your DAW and rescan your plugin folders.</p>
<ul>
  <li>Audio Units &rarr; <code>/Library/Audio/Plug-Ins/Components</code></li>
  <li>VST3 &rarr; <code>/Library/Audio/Plug-Ins/VST3</code></li>
</ul>
</body></html>"

# ── Final product package ────────────────────────────────────────────────────
OUTPUT_PKG="$OUTPUT_DIR/${INSTALLER_NAME}-${VERSION}.pkg"

echo "==> Building final installer: $OUTPUT_PKG"
productbuild \
    --distribution "$DIST_XML" \
    --resources    "$RESOURCES_DIR" \
    --package-path "$PKG_DIR" \
    "$OUTPUT_PKG"

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "  Installer ready: $OUTPUT_PKG"
echo ""
echo "  Plugins included: ${PLUGINS[*]}"
echo "  Formats included: ${FORMATS[*]}"
echo ""
echo "  Open with:    open \"$OUTPUT_PKG\""
echo "  Or install:   sudo installer -pkg \"$OUTPUT_PKG\" -target /"
echo "════════════════════════════════════════════════════════════════════"
