#!/usr/bin/env node
/*
 * Generates the React landing assets from the original pueski static site.
 * Extracts CSS / body-markup / main script, rewrites asset paths to /landing/...,
 * turns the free download links into login-gated CTAs, and emits three files
 * under src/pages/landing/. Re-run if the source site changes.
 *
 *   node scripts/gen-landing.js /path/to/pueski/index.html
 */
const fs = require("fs");
const path = require("path");

// ---- light-theme inversion: flip each color's LIGHTNESS (dark<->light) but keep
//      its hue/saturation, so the dark scheme becomes a light one while the cyan
//      brand accent stays cyan. Set INVERT_LIGHTNESS=false to ship the original dark scheme.
const INVERT_LIGHTNESS = true;

function rgbToHsl(r, g, b) {
  r /= 255; g /= 255; b /= 255;
  const max = Math.max(r, g, b), min = Math.min(r, g, b);
  let h = 0, s = 0; const l = (max + min) / 2;
  const d = max - min;
  if (d !== 0) {
    s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
    switch (max) {
      case r: h = (g - b) / d + (g < b ? 6 : 0); break;
      case g: h = (b - r) / d + 2; break;
      default: h = (r - g) / d + 4;
    }
    h /= 6;
  }
  return [h, s, l];
}
function hslToRgb(h, s, l) {
  let r, g, b;
  if (s === 0) { r = g = b = l; }
  else {
    const hue2rgb = (p, q, t) => {
      if (t < 0) t += 1; if (t > 1) t -= 1;
      if (t < 1 / 6) return p + (q - p) * 6 * t;
      if (t < 1 / 2) return q;
      if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
      return p;
    };
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const p = 2 * l - q;
    r = hue2rgb(p, q, h + 1 / 3); g = hue2rgb(p, q, h); b = hue2rgb(p, q, h - 1 / 3);
  }
  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}
function invLight(r, g, b) {
  const [h, s, l] = rgbToHsl(r, g, b);
  return hslToRgb(h, s, 1 - l);
}
function hex2(n) { return n.toString(16).padStart(2, "0"); }
function invertColorsInString(str, includeHexLiteral0x) {
  if (!INVERT_LIGHTNESS) return str;
  // #rgb / #rrggbb
  str = str.replace(/#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})\b/g, (m, hex) => {
    let r, g, b;
    if (hex.length === 3) {
      r = parseInt(hex[0] + hex[0], 16); g = parseInt(hex[1] + hex[1], 16); b = parseInt(hex[2] + hex[2], 16);
    } else {
      r = parseInt(hex.slice(0, 2), 16); g = parseInt(hex.slice(2, 4), 16); b = parseInt(hex.slice(4, 6), 16);
    }
    const [nr, ng, nb] = invLight(r, g, b);
    return "#" + hex2(nr) + hex2(ng) + hex2(nb);
  });
  // rgb(...) / rgba(...)
  str = str.replace(/rgba?\(([^)]+)\)/g, (m, inner) => {
    const parts = inner.split(",").map((s) => s.trim());
    if (parts.length < 3) return m;
    const r = parseFloat(parts[0]), g = parseFloat(parts[1]), b = parseFloat(parts[2]);
    if ([r, g, b].some((n) => Number.isNaN(n))) return m;
    const [nr, ng, nb] = invLight(r, g, b);
    return (parts.length >= 4 ? "rgba(" : "rgb(") + [nr, ng, nb].concat(parts.slice(3)).join(", ") + ")";
  });
  // 0xRRGGBB (three.js colors) — only where explicitly requested (script), never in markup text
  if (includeHexLiteral0x) {
    str = str.replace(/0x([0-9a-fA-F]{6})\b/g, (m, hex) => {
      const r = parseInt(hex.slice(0, 2), 16), g = parseInt(hex.slice(2, 4), 16), b = parseInt(hex.slice(4, 6), 16);
      const [nr, ng, nb] = invLight(r, g, b);
      return "0x" + hex2(nr) + hex2(ng) + hex2(nb);
    });
  }
  return str;
}

// Remove a <div ...>...</div> block (with nested divs) starting at the first
// occurrence of `openTagPrefix`, by counting div depth.
function removeBalancedDiv(html, openTagPrefix) {
  const start = html.indexOf(openTagPrefix);
  if (start === -1) return html;
  const re = /<div\b|<\/div>/g;
  re.lastIndex = start;
  let depth = 0, end = -1, m;
  while ((m = re.exec(html))) {
    if (m[0] === "</div>") { depth--; if (depth === 0) { end = re.lastIndex; break; } }
    else depth++;
  }
  if (end === -1) return html;
  return html.slice(0, start) + html.slice(end);
}

const SRC = process.argv[2] || "/Users/mpue/Documents/devel/pueski/index.html";
const OUT = path.resolve(__dirname, "..", "src", "pages", "landing");
fs.mkdirSync(OUT, { recursive: true });

const html = fs.readFileSync(SRC, "utf8");

// ---- extract the three parts ----
const styleStart = html.indexOf("<style>") + "<style>".length;
const styleEnd = html.indexOf("</style>", styleStart);
let css = html.slice(styleStart, styleEnd);

const bodyStart = html.indexOf("<body>") + "<body>".length;
const appEnd = html.indexOf("</div><!-- #app -->") + "</div><!-- #app -->".length;
let body = html.slice(bodyStart, appEnd);

const threeIdx = html.indexOf("three.min.js");
const scriptStart = html.indexOf("<script>", threeIdx) + "<script>".length;
const scriptEnd = html.indexOf("</script>", scriptStart);
let script = html.slice(scriptStart, scriptEnd);

// ---- rewrite asset paths ----
const toLandingImages = (s) => s.split("images/").join("/landing/images/");
css = toLandingImages(css);
body = toLandingImages(body);

// audio: prefix load paths but keep clean display names
script = script
  .split("new Audio('music.mp3')").join("new Audio('/landing/audio/music.mp3')")
  .split("loadAudio('music.mp3','music')").join("loadAudio('/landing/audio/music.mp3','music')")
  .split("sfxLoad(k,fn,fn)").join("sfxLoad(k,'/landing/audio/'+fn,fn)");

// ---- gate downloads: free DMG/PDF links -> login-gated CTAs ----
body = body.replace(/href="downloads\/[^"]*"(\s+download)?/g, 'href="#" data-nav="/profile"');

// ---- drop the Profil / Projekte / Über-das-Bundle sections + their nav entries,
//      and add a Shop link (-> React /shop route, intercepted via data-nav) ----
body = body.replace(
  /<button class="nav-link"[^>]*showSec\('(?:me|projects|about)'[^>]*>[^<]*<\/button>\s*/g,
  ""
);
["sec-me", "sec-projects", "sec-about"].forEach((id) => {
  body = removeBalancedDiv(body, '<div id="' + id + '"');
});
body = body
  .replace(
    `<button class="nav-link" onclick="showSec('hero',this)">Start</button>`,
    `<button class="nav-link" onclick="showSec('hero',this)">Start</button>\n    <button class="nav-link" data-nav="/shop">Shop</button>`
  )
  .replace(
    `<button class="nav-link" onclick="showSec('hero',this);closeBurger()">Start</button>`,
    `<button class="nav-link" onclick="showSec('hero',this);closeBurger()">Start</button>\n  <button class="nav-link" data-nav="/shop">Shop</button>`
  );

// ---- make skipIntro idempotent (it can fire twice: pct-counter path + the
//      4.8s auto-fire). Originally that double-call null-derefs once #intro is
//      gone — harmless on the static page, but the React dev overlay surfaces it.
script = script
  .replace(
    "document.getElementById('intro').classList.add('out');",
    "var _intro=document.getElementById('intro'); if(!_intro) return; _intro.classList.add('out');"
  )
  .replace(
    "document.getElementById('intro').remove();",
    "document.getElementById('intro')?.remove();"
  );

// ---- light-theme color inversion (lightness-flip, hue preserved) ----
css = invertColorsInString(css, false);
body = invertColorsInString(body, false); // inline style= colors; 0x left alone (markup has "0x4D4154" as text)
script = invertColorsInString(script, true); // includes three.js 0xRRGGBB color literals

// ---- emit ----
const banner = "// AUTO-GENERATED from the pueski static site by scripts/gen-landing.js — do not edit by hand.\n";

fs.writeFileSync(
  path.join(OUT, "landingHtml.ts"),
  banner + "export const LANDING_HTML = " + JSON.stringify(body) + ";\n"
);
fs.writeFileSync(
  path.join(OUT, "landingCss.ts"),
  banner + "export const LANDING_CSS = " + JSON.stringify(css) + ";\n"
);

const PREAMBLE = `${banner}// @ts-nocheck
/* The original pueski page script runs verbatim below, wrapped so every global
   listener / rAF / timer it registers is tracked and torn down when the React
   route unmounts (otherwise the cursor handler etc. would leak onto other pages). */
export function initLanding(): () => void {
  const _cleanups = [];
  const _rafIds = [];
  const _intervalIds = [];
  const _timeoutIds = [];

  const _origDocAdd = document.addEventListener;
  const _origDocRem = document.removeEventListener;
  const _origWinAdd = window.addEventListener;
  const _origWinRem = window.removeEventListener;
  const _origRaf = window.requestAnimationFrame;
  const _origCaf = window.cancelAnimationFrame;
  const _origSi = window.setInterval;
  const _origCi = window.clearInterval;
  const _origSt = window.setTimeout;
  const _origCt = window.clearTimeout;

  document.addEventListener = function (t, f, o) {
    _origDocAdd.call(document, t, f, o);
    _cleanups.push(() => _origDocRem.call(document, t, f, o));
  };
  window.addEventListener = function (t, f, o) {
    _origWinAdd.call(window, t, f, o);
    _cleanups.push(() => _origWinRem.call(window, t, f, o));
  };
  window.requestAnimationFrame = function (cb) {
    const id = _origRaf.call(window, cb);
    _rafIds.push(id);
    return id;
  };
  window.setInterval = function (fn, ms) {
    const id = _origSi.call(window, fn, ms);
    _intervalIds.push(id);
    return id;
  };
  window.setTimeout = function (fn, ms) {
    const id = _origSt.call(window, fn, ms);
    _timeoutIds.push(id);
    return id;
  };

  try {
/* ===================== BEGIN VERBATIM PUESKI SCRIPT ===================== */
`;

const POSTAMBLE = `
    // Expose handlers referenced by inline onclick/oninput in the markup. MUST be
    // inside this try block: in strict-mode ES modules, function declarations in a
    // block are block-scoped, so the pueski script's functions are only visible here.
    Object.assign(window, {
      skipIntro, showSec, toggleBurger, closeBurger, sfxPlay, togglePlay, setVol, closeDetail,
    });
/* ====================== END VERBATIM PUESKI SCRIPT ====================== */
  } catch (e) {
    console.error("landing init error", e);
  }

  return function teardown() {
    window.requestAnimationFrame = _origRaf;
    window.setInterval = _origSi;
    window.setTimeout = _origSt;
    document.addEventListener = _origDocAdd;
    window.addEventListener = _origWinAdd;
    _rafIds.forEach((id) => { try { _origCaf.call(window, id); } catch (e) {} });
    _intervalIds.forEach((id) => { try { _origCi.call(window, id); } catch (e) {} });
    _timeoutIds.forEach((id) => { try { _origCt.call(window, id); } catch (e) {} });
    _cleanups.forEach((fn) => { try { fn(); } catch (e) {} });
    ["skipIntro","showSec","toggleBurger","closeBurger","sfxPlay","togglePlay","setVol","closeDetail"]
      .forEach((k) => { try { delete window[k]; } catch (e) {} });
  };
}
`;

fs.writeFileSync(path.join(OUT, "landingScript.ts"), PREAMBLE + script + POSTAMBLE);

console.log("Wrote landingHtml.ts (%d bytes), landingCss.ts (%d bytes), landingScript.ts",
  body.length, css.length);
