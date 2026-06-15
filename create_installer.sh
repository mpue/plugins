#!/usr/bin/env bash
# Builds macOS .pkg installers for the plugins in this repo.
#
# Default: ONE suite installer bundling every selected plugin as a component
# package with a customisable choice tree: top-level groups by format
# (AU / VST3), one toggle per plugin inside each group.
#
# Individual plugins:
#   • --plugins with a SINGLE plugin produces an installer named after that
#     plugin (e.g. Pike-1.0.0.pkg) instead of the suite installer
#   • --each produces one standalone installer per selected plugin
#
# Usage:
#   ./create_installer.sh                              # suite, version 1.0.0
#   ./create_installer.sh 2.1.0                        # explicit version
#   ./create_installer.sh 2.1.0 --plugins AF-1,Lupo    # suite with subset
#   ./create_installer.sh 2.1.0 --plugins Pike         # single-plugin installer
#   ./create_installer.sh 2.1.0 --each                 # one installer per plugin
#   ./create_installer.sh 2.1.0 --plugins AF-1,Lupo --each
#   ./create_installer.sh 2.1.0 --formats VST3         # AU or VST3 only
#   ./create_installer.sh 2.1.0 --name MyBundle        # override name/title
#
# Requires the plugins to have been built into
#   <plugin>/Builds/MacOSX/build/Release/<plugin>.{component,vst3}
# (use ./build_all.sh first, or run ./make_release.sh which does both).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ALL_PLUGINS=(AF-1 ARP-1 BC-1 BS-1 CD-1 CH-1 CP-1 CRV-1 DL-1 EQ8 FP-1 GS-1 HH-1 KM-1 LT-1 Lupo PM-1 PS-1 Pike RV-1 SA-1 SN-1 ST-1 SW-1 TR-1 TS-1)
ALL_FORMATS=(AU VST3)

# ── Args ─────────────────────────────────────────────────────────────────────
VERSION="1.0.0"
PLUGINS=()
FORMATS=()
NAME=""
EACH=0

# First positional arg (if it doesn't start with --) is the version.
if [[ $# -gt 0 && "$1" != --* ]]; then
    VERSION="$1"; shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --plugins)  IFS=',' read -r -a PLUGINS <<< "$2"; shift 2 ;;
        --formats)  IFS=',' read -r -a FORMATS <<< "$2"; shift 2 ;;
        --name)     NAME="$2"; shift 2 ;;
        --each)     EACH=1; shift ;;
        -h|--help)  sed -n '2,26p' "$0"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

[[ ${#PLUGINS[@]} -eq 0 ]] && PLUGINS=("${ALL_PLUGINS[@]}")
[[ ${#FORMATS[@]} -eq 0 ]] && FORMATS=("${ALL_FORMATS[@]}")

if (( EACH )) && [[ -n "$NAME" ]]; then
    echo "ERROR: --name cannot be combined with --each (each installer is named after its plugin)." >&2
    exit 2
fi

INSTALLER_DIR="$SCRIPT_DIR/Installer"
STAGING_DIR="$INSTALLER_DIR/staging"
PKG_DIR="$INSTALLER_DIR/packages"
OUTPUT_DIR="$INSTALLER_DIR/output"

SUITE_NAME="Pueski-Plugins"
SUITE_TITLE="Pueski Plugin Suite"

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
mkdir -p "$STAGING_DIR" "$PKG_DIR" "$OUTPUT_DIR"

# ── Component .pkg per (plugin, format) — shared across installers ───────────
build_component_pkg() {
    local plugin="$1" format="$2"
    local art bid sub stage_root
    local pkg_filename="${plugin}-${format}.pkg"

    [[ -f "$PKG_DIR/$pkg_filename" ]] && return 0   # already built this run

    art=$(artifact_for "$plugin" "$format")
    bid=$(bundle_id "$plugin" "$format")
    sub=$(install_subdir_for "$format")

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
}

# ── One product installer for a named set of plugins ─────────────────────────
OUTPUT_PKGS=()

build_installer() {
    local name="$1" title="$2"; shift 2
    local plugins=("$@")

    local dist_xml="$STAGING_DIR/distribution-$name.xml"
    local res_dir="$STAGING_DIR/resources-$name"
    mkdir -p "$res_dir"

    local CHOICE_LINES_AU=() CHOICE_LINES_VST3=() CHOICE_BLOCKS=() PKG_REF_BLOCKS=()
    local plugin format art bid sub pkg_filename choice_id desc

    for plugin in "${plugins[@]}"; do
        for format in "${FORMATS[@]}"; do
            build_component_pkg "$plugin" "$format"

            bid=$(bundle_id "$plugin" "$format")
            sub=$(install_subdir_for "$format")
            pkg_filename="${plugin}-${format}.pkg"
            choice_id="${plugin//-/_}_${format}"

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
        done
    done

    # ── Format-group "parent" choices (toggling these toggles every child) ───
    local GROUP_CHOICES="" OUTLINE="" have_au=0 have_vst3=0 f l
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

    # ── distribution.xml ──────────────────────────────────────────────────────
    {
        echo '<?xml version="1.0" encoding="utf-8"?>'
        echo '<installer-gui-script minSpecVersion="2">'
        echo "    <title>$(xml_escape "$title") $VERSION</title>"
        echo '    <welcome  file="welcome.html"  mime-type="text/html" />'
        echo '    <license  file="license.html"  mime-type="text/html" />'
        echo '    <readme   file="readme.html"   mime-type="text/html" />'
        echo '    <options customize="allow" require-scripts="false" rootVolumeOnly="false"/>'
        echo '    <choices-outline>'
        printf '%s' "$OUTLINE"
        echo '    </choices-outline>'
        printf '%s\n' "$GROUP_CHOICES"
        local c r
        for c in "${CHOICE_BLOCKS[@]}"; do
            printf '%s\n' "$c"
        done
        for r in "${PKG_REF_BLOCKS[@]}"; do
            printf '%s\n' "$r"
        done
        echo '</installer-gui-script>'
    } > "$dist_xml"

    # ── HTML resources (regenerated per installer so the list never goes stale)
    local YEAR PLUGIN_LIST_HTML="" p
    YEAR=$(date +%Y)
    for p in "${plugins[@]}"; do
        PLUGIN_LIST_HTML+="<li>$p</li>"
    done

    if [[ ${#plugins[@]} -eq 1 ]]; then
        cat > "$res_dir/welcome.html" <<HTML
<html><body>
<h2>$title $VERSION</h2>
<p>This installer contains the <b>${plugins[0]}</b> plugin by Matthias Pueski.</p>
<p>On the next screens you can pick the formats (Audio Unit / VST3) you wish
to install.</p>
</body></html>
HTML
    else
        cat > "$res_dir/welcome.html" <<HTML
<html><body>
<h2>$title $VERSION</h2>
<p>This installer bundles the following plugins by Matthias Pueski:</p>
<ul>$PLUGIN_LIST_HTML</ul>
<p>On the next screens you can pick the formats (Audio Unit / VST3) and the
individual plugins you wish to install.</p>
</body></html>
HTML
    fi

    cat > "$res_dir/license.html" <<HTML
<html><body>
<h2>License</h2>
<p>Copyright &copy; $YEAR Matthias Pueski. All rights reserved.</p>
<p>This software is provided &quot;as is&quot;, without warranty of any kind.</p>
</body></html>
HTML

    cat > "$res_dir/readme.html" <<HTML
<html><body>
<h2>$title $VERSION &mdash; Read Me</h2>
<p>After installation, restart your DAW and rescan your plugin folders.</p>
<ul>
  <li>Audio Units &rarr; <code>/Library/Audio/Plug-Ins/Components</code></li>
  <li>VST3 &rarr; <code>/Library/Audio/Plug-Ins/VST3</code></li>
</ul>
</body></html>
HTML

    # ── Final product package ─────────────────────────────────────────────────
    local out_pkg="$OUTPUT_DIR/${name}-${VERSION}.pkg"
    echo "==> Building installer: $out_pkg"
    productbuild \
        --distribution "$dist_xml" \
        --resources    "$res_dir" \
        --package-path "$PKG_DIR" \
        "$out_pkg"
    OUTPUT_PKGS+=("$out_pkg")
}

# ── Build the requested installer(s) ─────────────────────────────────────────
if (( EACH )); then
    for plugin in "${PLUGINS[@]}"; do
        build_installer "$plugin" "$plugin" "$plugin"
    done
else
    if [[ -n "$NAME" ]]; then
        build_installer "$NAME" "$NAME" "${PLUGINS[@]}"
    elif [[ ${#PLUGINS[@]} -eq 1 ]]; then
        build_installer "${PLUGINS[0]}" "${PLUGINS[0]}" "${PLUGINS[@]}"
    else
        build_installer "$SUITE_NAME" "$SUITE_TITLE" "${PLUGINS[@]}"
    fi
fi

echo ""
echo "════════════════════════════════════════════════════════════════════"
if [[ ${#OUTPUT_PKGS[@]} -eq 1 ]]; then
    echo "  Installer ready: ${OUTPUT_PKGS[0]}"
else
    echo "  ${#OUTPUT_PKGS[@]} installers ready:"
    printf '    %s\n' "${OUTPUT_PKGS[@]}"
fi
echo ""
echo "  Plugins included: ${PLUGINS[*]}"
echo "  Formats included: ${FORMATS[*]}"
echo ""
echo "  Open with:    open \"${OUTPUT_PKGS[0]}\""
echo "  Or install:   sudo installer -pkg \"${OUTPUT_PKGS[0]}\" -target /"
echo "════════════════════════════════════════════════════════════════════"
