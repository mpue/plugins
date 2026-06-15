import React, { useEffect, useRef } from "react";
import { useNavigate } from "react-router-dom";
import { LANDING_HTML } from "./landingHtml";
import { LANDING_CSS } from "./landingCss";
import { initLanding } from "./landingScript";

// The pueski hero uses Three.js r128 (matches the original page). Loaded lazily
// so it only costs anything on the landing route; the page degrades gracefully
// (just no wireframe sphere) if it fails to load.
const THREE_SRC =
  "https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js";

function ensureThree(): Promise<void> {
  return new Promise((resolve) => {
    if ((window as any).THREE) return resolve();
    const existing = document.getElementById("three-cdn") as HTMLScriptElement | null;
    if (existing) {
      existing.addEventListener("load", () => resolve(), { once: true });
      existing.addEventListener("error", () => resolve(), { once: true });
      return;
    }
    const s = document.createElement("script");
    s.id = "three-cdn";
    s.src = THREE_SRC;
    s.onload = () => resolve();
    s.onerror = () => resolve();
    document.head.appendChild(s);
  });
}

/**
 * The ported pueski marketing page, rendered as the app's landing route.
 * The original markup/CSS/JS run inside this component; CSS is injected only
 * while mounted (no bleed onto other routes) and all the page's global
 * listeners/timers are torn down on unmount via initLanding()'s teardown.
 */
export default function Landing() {
  const ref = useRef<HTMLDivElement>(null);
  const navigate = useNavigate();

  useEffect(() => {
    const root = ref.current;
    if (!root) return;

    root.innerHTML = LANDING_HTML;

    const styleEl = document.createElement("style");
    styleEl.setAttribute("data-landing", "");
    styleEl.textContent = LANDING_CSS;
    document.head.appendChild(styleEl);

    // CTAs in the markup carry data-nav="/profile" etc. — route them through
    // React Router (login-gated downloads) instead of hard navigation.
    const onClick = (e: MouseEvent) => {
      const el = (e.target as HTMLElement).closest("[data-nav]");
      if (el) {
        e.preventDefault();
        navigate(el.getAttribute("data-nav") || "/");
      }
    };
    root.addEventListener("click", onClick);

    let teardown: () => void = () => {};
    let cancelled = false;
    ensureThree().then(() => {
      if (!cancelled) teardown = initLanding();
    });

    return () => {
      cancelled = true;
      root.removeEventListener("click", onClick);
      try {
        teardown();
      } catch (e) {
        /* ignore */
      }
      styleEl.remove();
      root.innerHTML = "";
    };
  }, [navigate]);

  return <div ref={ref} className="pueski-landing-root" />;
}
