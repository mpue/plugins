import React, { useEffect, useRef, useState } from "react";
import { api, apiUpload, assetUrl, getToken } from "../../api";

interface ProductFile {
  id: string;
  platform: "MAC" | "WINDOWS";
  format: "AU" | "VST3";
  fileSize: number;
}

interface Product {
  id: string;
  slug: string;
  name: string;
  description: string;
  priceCents: number;
  currency: string;
  imageUrl: string | null;
  productFiles: ProductFile[];
  version: string;
  active: boolean;
}

const blank = {
  slug: "",
  name: "",
  description: "",
  priceCents: 0,
  currency: "EUR",
  version: "1.0.0",
  active: true,
};

export default function AdminProducts() {
  const [products, setProducts] = useState<Product[]>([]);
  const [editing, setEditing] = useState<Partial<Product> | null>(null);
  const [error, setError] = useState<string | null>(null);
  const imgRef = useRef<HTMLInputElement>(null);
  const [uploadState, setUploadState] = useState<{ productId: string; platform: "MAC" | "WINDOWS"; format: "AU" | "VST3" } | null>(null);
  const pluginFileRef = useRef<HTMLInputElement>(null);

  async function load() {
    try {
      setProducts(await api<Product[]>("/admin/products"));
    } catch (e: any) {
      setError(e.message);
    }
  }
  useEffect(() => {
    load();
  }, []);

  async function save() {
    if (!editing) return;
    setError(null);
    try {
      const payload: any = {
        slug: editing.slug,
        name: editing.name,
        description: editing.description,
        priceCents: Number(editing.priceCents) || 0,
        currency: editing.currency,
        version: editing.version,
        active: editing.active,
      };
      if (editing.id) {
        await api(`/admin/products/${editing.id}`, {
          method: "PATCH",
          body: JSON.stringify(payload),
        });
      } else {
        await api("/admin/products", {
          method: "POST",
          body: JSON.stringify(payload),
        });
      }
      setEditing(null);
      await load();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function remove(id: string) {
    if (!confirm("Delete this product?")) return;
    await api(`/admin/products/${id}`, { method: "DELETE" });
    await load();
  }

  async function uploadImage(id: string, file: File) {
    await apiUpload(`/admin/products/${id}/image`, file);
    await load();
  }

  async function uploadPluginFile(productId: string, platform: string, format: string, file: File) {
    const fd = new FormData();
    fd.append("file", file);
    fd.append("platform", platform);
    fd.append("format", format);
    const token = getToken();
    const API_BASE = process.env.API_BASE_URL || "";
    const res = await fetch(`${API_BASE}/api/admin/products/${productId}/files`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: fd,
    });
    if (!res.ok) {
      const d = await res.json().catch(() => ({}));
      throw new Error(d.error || `HTTP ${res.status}`);
    }
    await load();
  }

  async function deletePluginFile(productId: string, fileId: string) {
    if (!confirm("Delete this format file?")) return;
    await api(`/admin/products/${productId}/files/${fileId}`, { method: "DELETE" });
    await load();
  }

  return (
    <div>
      {error && <div className="alert error">{error}</div>}
      <button className="btn" onClick={() => setEditing({ ...blank })}>
        New product
      </button>
      <hr />

      {editing && (
        <div className="card">
          <h3>{editing.id ? "Edit product" : "New product"}</h3>
          <div className="field">
            <label>Slug</label>
            <input
              className="input"
              value={editing.slug || ""}
              onChange={(e) => setEditing({ ...editing, slug: e.target.value })}
            />
          </div>
          <div className="field">
            <label>Name</label>
            <input
              className="input"
              value={editing.name || ""}
              onChange={(e) => setEditing({ ...editing, name: e.target.value })}
            />
          </div>
          <div className="field">
            <label>Description</label>
            <textarea
              className="textarea"
              value={editing.description || ""}
              onChange={(e) => setEditing({ ...editing, description: e.target.value })}
            />
          </div>
          <div className="row">
            <div className="field" style={{ flex: 1 }}>
              <label>Price (cents)</label>
              <input
                className="input"
                type="number"
                value={editing.priceCents ?? 0}
                onChange={(e) => setEditing({ ...editing, priceCents: Number(e.target.value) })}
              />
            </div>
            <div className="field" style={{ width: 100 }}>
              <label>Currency</label>
              <input
                className="input"
                value={editing.currency || "EUR"}
                onChange={(e) => setEditing({ ...editing, currency: e.target.value.toUpperCase() })}
              />
            </div>
            <div className="field" style={{ width: 130 }}>
              <label>Version</label>
              <input
                className="input"
                value={editing.version || ""}
                onChange={(e) => setEditing({ ...editing, version: e.target.value })}
              />
            </div>
          </div>
          <div className="field">
            <label>
              <input
                type="checkbox"
                checked={!!editing.active}
                onChange={(e) => setEditing({ ...editing, active: e.target.checked })}
              />{" "}
              Active
            </label>
          </div>
          <div className="row">
            <button className="btn" onClick={save}>Save</button>
            <button className="btn secondary" onClick={() => setEditing(null)}>Cancel</button>
          </div>
        </div>
      )}

      <table>
        <thead>
          <tr>
            <th>Image</th>
            <th>Name / Slug</th>
            <th>Price</th>
            <th>Version</th>
            <th>File</th>
            <th>Status</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {products.map((p) => (
            <tr key={p.id}>
              <td>
                {p.imageUrl ? (
                  <img src={assetUrl(p.imageUrl)} style={{ width: 60, height: 40, objectFit: "cover" }} />
                ) : (
                  <span className="muted">—</span>
                )}
              </td>
              <td>
                <strong>{p.name}</strong>
                <div className="muted">{p.slug}</div>
              </td>
              <td>
                {(p.priceCents / 100).toFixed(2)} {p.currency}
              </td>
              <td>{p.version}</td>
              <td>
                {p.productFiles.length === 0 ? (
                  <span className="muted">none</span>
                ) : p.productFiles.map((f) => (
                  <span key={f.id} className="tag active" style={{ marginRight: 4, cursor: "default" }}>
                    {f.platform === "MAC" ? "macOS" : "Win"} {f.format} ({(f.fileSize / 1024 / 1024).toFixed(1)} MB)
                  </span>
                ))}
              </td>
              <td>
                <span className={`tag ${p.active ? "active" : "revoked"}`}>
                  {p.active ? "active" : "inactive"}
                </span>
              </td>
              <td>
                <div className="row">
                  <button className="btn secondary" onClick={() => setEditing(p)}>
                    Edit
                  </button>
                  <button
                    className="btn secondary"
                    onClick={() => imgRef.current?.click()}
                    onMouseDown={() => imgRef.current?.setAttribute("data-pid", p.id)}
                  >
                    Image
                  </button>
                  <UploadFormatButtons
                    product={p}
                    onUpload={(platform, format, file) => uploadPluginFile(p.id, platform, format, file).catch((e) => setError(e.message))}
                    onDelete={(fileId) => deletePluginFile(p.id, fileId).catch((e) => setError(e.message))}
                  />
                  <button className="btn danger" onClick={() => remove(p.id)}>
                    Delete
                  </button>
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>

      <input
        ref={imgRef}
        type="file"
        accept="image/*"
        style={{ display: "none" }}
        onChange={(e) => {
          const id = imgRef.current?.getAttribute("data-pid") || "";
          const f = e.target.files?.[0];
          if (id && f) uploadImage(id, f);
          e.target.value = "";
        }}
      />
    </div>
  );
}

const FORMATS: { platform: "MAC" | "WINDOWS"; format: "AU" | "VST3"; label: string }[] = [
  { platform: "MAC", format: "AU", label: "macOS AU" },
  { platform: "MAC", format: "VST3", label: "macOS VST3" },
  { platform: "WINDOWS", format: "VST3", label: "Win VST3" },
];

function UploadFormatButtons({
  product,
  onUpload,
  onDelete,
}: {
  product: Product;
  onUpload: (platform: "MAC" | "WINDOWS", format: "AU" | "VST3", file: File) => void;
  onDelete: (fileId: string) => void;
}) {
  const refs = useRef<Record<string, HTMLInputElement | null>>({});

  return (
    <>
      {FORMATS.map(({ platform, format, label }) => {
        const key = `${platform}-${format}`;
        const existing = product.productFiles.find((f) => f.platform === platform && f.format === format);
        return (
          <span key={key} style={{ display: "inline-flex", gap: 2 }}>
            <button
              className="btn secondary"
              title={existing ? `Replace ${label}` : `Upload ${label}`}
              onClick={() => refs.current[key]?.click()}
            >
              {existing ? `↺ ${label}` : `+ ${label}`}
            </button>
            {existing && (
              <button className="btn danger" title={`Delete ${label}`} onClick={() => onDelete(existing.id)}>
                ✕
              </button>
            )}
            <input
              ref={(el) => { refs.current[key] = el; }}
              type="file"
              style={{ display: "none" }}
              onChange={(e) => {
                const f = e.target.files?.[0];
                if (f) onUpload(platform, format, f);
                e.target.value = "";
              }}
            />
          </span>
        );
      })}
    </>
  );
}
