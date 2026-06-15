import React, { useEffect, useState } from "react";
import { api, downloadUrl } from "../api";
import { useAuth } from "../auth";

interface Activation {
  id: string;
  machineId: string;
  appVersion: string | null;
  createdAt: string;
}

interface License {
  id: string;
  key: string;
  status: string;
  expiresAt: string | null;
  createdAt: string;
  maxActivations: number;
  activations: Activation[];
  product: {
    id: string;
    slug: string;
    name: string;
    version: string;
    productFiles: { id: string; platform: string; format: string; fileSize: number }[];
  };
}

export default function Profile() {
  const { user, refresh } = useAuth();
  const [licenses, setLicenses] = useState<License[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [apiToken, setApiToken] = useState<string | null>(null);

  async function loadLicenses() {
    try {
      const data = await api<License[]>("/licenses");
      setLicenses(data);
    } catch (e: any) {
      setError(e.message);
    }
  }

  useEffect(() => {
    loadLicenses();
  }, []);

  async function startDownload(licenseId: string, fileId?: string) {
    try {
      const query = fileId ? `?fileId=${encodeURIComponent(fileId)}` : "";
      const res = await api<{ token: string }>(`/downloads/token/${licenseId}${query}`, {
        method: "POST",
      });
      window.location.href = downloadUrl(res.token);
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function generateApiToken() {
    const res = await api<{ apiToken: string }>("/auth/api-token", { method: "POST" });
    setApiToken(res.apiToken);
    await refresh();
  }

  async function revokeApiToken() {
    await api("/auth/api-token", { method: "DELETE" });
    setApiToken(null);
    await refresh();
  }

  return (
    <div className="container">
      <h1>My Profile</h1>
      <div className="card">
        <p>
          <strong>{user?.name || "—"}</strong>
        </p>
        <p className="muted">{user?.email}</p>
        <p className="muted">Role: {user?.role}</p>
      </div>

      <h2>Installer API token</h2>
      <div className="card">
        <p className="muted">
          Use this token in the native installer (Authorization: Bearer …) to access your products.
        </p>
        {apiToken && (
          <div className="alert success" style={{ wordBreak: "break-all" }}>
            New token (copy now — shown only once): <code>{apiToken}</code>
          </div>
        )}
        <div className="row">
          <button className="btn" onClick={generateApiToken}>
            {user?.hasApiToken ? "Regenerate token" : "Generate token"}
          </button>
          {user?.hasApiToken && (
            <button className="btn danger" onClick={revokeApiToken}>
              Revoke
            </button>
          )}
        </div>
      </div>

      <h2>My Licenses & Downloads</h2>
      {error && <div className="alert error">{error}</div>}
      {licenses.length === 0 ? (
        <p className="muted">No licenses yet. Visit the shop to purchase plugins.</p>
      ) : (
        <table>
          <thead>
            <tr>
              <th>Product</th>
              <th>Version</th>
              <th>License Key</th>
              <th>Status</th>
              <th>Devices</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {licenses.map((l) => (
              <tr key={l.id}>
                <td>{l.product.name}</td>
                <td>{l.product.version}</td>
                <td>
                  <code>{l.key}</code>
                </td>
                <td>
                  <span
                    className={`tag ${l.status === "ACTIVE" ? "active" : "revoked"}`}
                  >
                    {l.status}
                  </span>
                </td>
                <td>
                  {l.activations.length === 0 ? (
                    <span className="muted">0 / {l.maxActivations}</span>
                  ) : (
                    <details>
                      <summary>
                        {l.activations.length} / {l.maxActivations}
                      </summary>
                      <ul style={{ margin: "6px 0 0", paddingLeft: 16 }}>
                        {l.activations.map((a) => (
                          <li
                            key={a.id}
                            className="muted"
                            style={{ fontSize: "0.8rem" }}
                          >
                            <code>{a.machineId.slice(0, 12)}…</code>
                            {a.appVersion ? ` · v${a.appVersion}` : ""} ·{" "}
                            {new Date(a.createdAt).toLocaleDateString()}
                          </li>
                        ))}
                      </ul>
                    </details>
                  )}
                </td>
                <td>
                  {l.product.productFiles.length === 0 ? (
                    <span className="muted">No files</span>
                  ) : (
                    <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
                      {l.product.productFiles.map((f) => (
                        <button
                          key={f.id}
                          className="btn"
                          style={{ fontSize: "0.8rem", padding: "4px 8px" }}
                          onClick={() => startDownload(l.id, f.id)}
                          disabled={l.status !== "ACTIVE"}
                          title={`${(f.fileSize / 1024 / 1024).toFixed(1)} MB`}
                        >
                          ↓ {f.platform === "MAC" ? "macOS" : "Win"} {f.format}
                        </button>
                      ))}
                    </div>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}
