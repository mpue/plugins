import React, { useEffect, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import { api, assetUrl } from "../api";
import { useAuth } from "../auth";

interface Product {
  id: string;
  slug: string;
  name: string;
  description: string;
  priceCents: number;
  currency: string;
  imageUrl: string | null;
  version: string;
  productFiles: { id: string; platform: string; format: string; fileSize: number }[];
}

export default function ProductPage() {
  const { slug } = useParams();
  const { user } = useAuth();
  const navigate = useNavigate();
  const [product, setProduct] = useState<Product | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!slug) return;
    api<Product>(`/products/${slug}`)
      .then(setProduct)
      .catch((e) => setError(e.message));
  }, [slug]);

  async function buy() {
    if (!user) {
      navigate("/login", { state: { from: `/product/${slug}` } });
      return;
    }
    if (!product) return;
    setBusy(true);
    setError(null);
    try {
      const res = await api<{ url: string }>("/checkout/session", {
        method: "POST",
        body: JSON.stringify({ items: [{ productId: product.id, quantity: 1 }] }),
      });
      window.location.href = res.url;
    } catch (e: any) {
      setError(e.message);
      setBusy(false);
    }
  }

  if (!product && !error) return <div className="container">Loading…</div>;

  return (
    <div className="container">
      {error && <div className="alert error">{error}</div>}
      {product && (
        <div className="card" style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "1.5rem" }}>
          {product.imageUrl ? (
            <img
              src={assetUrl(product.imageUrl)}
              alt={product.name}
              style={{ width: "100%", borderRadius: 8 }}
            />
          ) : (
            <div style={{ height: 300, background: "var(--panel-2)", borderRadius: 8 }} />
          )}
          <div>
            <h1>{product.name}</h1>
            <p className="muted">v{product.version}</p>
            {product.productFiles.length > 0 && (
              <div style={{ marginBottom: 12 }}>
                <p className="muted" style={{ marginBottom: 4, fontSize: "0.85rem" }}>Available formats:</p>
                <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
                  {product.productFiles.map((f) => (
                    <span key={f.id} className="tag active">
                      {f.platform === "MAC" ? "macOS" : "Windows"} {f.format} ({(f.fileSize / 1024 / 1024).toFixed(1)} MB)
                    </span>
                  ))}
                </div>
              </div>
            )}
            <p style={{ whiteSpace: "pre-wrap" }}>{product.description}</p>
            <h2>{(product.priceCents / 100).toFixed(2)} {product.currency}</h2>
            <button className="btn" onClick={buy} disabled={busy}>
              {busy ? "Redirecting…" : "Buy now"}
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
