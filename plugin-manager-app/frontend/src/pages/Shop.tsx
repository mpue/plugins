import React, { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { api, assetUrl } from "../api";

interface Product {
  id: string;
  slug: string;
  name: string;
  description: string;
  priceCents: number;
  currency: string;
  imageUrl: string | null;
  version: string;
  productFiles: { id: string; platform: string; format: string }[];
}

export default function Shop() {
  const [products, setProducts] = useState<Product[]>([]);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    api<Product[]>("/products")
      .then(setProducts)
      .catch((e) => setError(e.message));
  }, []);

  return (
    <div className="container">
      <h1>Shop</h1>
      {error && <div className="alert error">{error}</div>}
      <div className="product-grid">
        {products.map((p) => (
          <Link
            key={p.id}
            to={`/product/${p.slug}`}
            className="product-card"
            style={{ color: "inherit" }}
          >
            {p.imageUrl ? (
              <img src={assetUrl(p.imageUrl)} alt={p.name} />
            ) : (
              <div style={{ height: 180, background: "var(--panel-2)" }} />
            )}
            <div className="body">
              <h3>{p.name}</h3>
              <p className="muted" style={{ fontSize: "0.88rem" }}>
                v{p.version}
              </p>
              {p.productFiles.length > 0 && (
                <div style={{ marginBottom: 6, display: "flex", gap: 4, flexWrap: "wrap" }}>
                  {p.productFiles.map((f) => (
                    <span key={f.id} className="tag active" style={{ fontSize: "0.75rem" }}>
                      {f.platform === "MAC" ? "macOS" : "Win"} {f.format}
                    </span>
                  ))}
                </div>
              )}
              <div className="price">
                {(p.priceCents / 100).toFixed(2)} {p.currency}
              </div>
            </div>
          </Link>
        ))}
        {products.length === 0 && !error && <p className="muted">No products yet.</p>}
      </div>
    </div>
  );
}
