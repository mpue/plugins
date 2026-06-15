import React, { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { api } from "../api";

export default function Home() {
  const [html, setHtml] = useState<string>("");
  const [tagline, setTagline] = useState<string>("");

  useEffect(() => {
    api<Record<string, string>>("/config/public")
      .then((cfg) => {
        setHtml(cfg["site.homeHtml"] || "<h1>Welcome</h1>");
        setTagline(cfg["site.tagline"] || "");
      })
      .catch(() => setHtml("<h1>Welcome</h1>"));
  }, []);

  return (
    <div className="container">
      {tagline && <p className="muted">{tagline}</p>}
      <div dangerouslySetInnerHTML={{ __html: html }} />
      <p>
        <Link className="btn" to="/shop">Browse the shop</Link>
      </p>
    </div>
  );
}
