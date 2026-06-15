import React, { useEffect, useState } from "react";
import { api } from "../../api";

interface Activation {
  id: string;
  machineId: string;
  appVersion: string | null;
  createdAt: string;
}

interface AdminLicense {
  id: string;
  key: string;
  status: "ACTIVE" | "REVOKED" | "EXPIRED";
  expiresAt: string | null;
  createdAt: string;
  maxActivations: number;
  activations: Activation[];
  user: { id: string; email: string };
  product: { id: string; name: string; slug: string };
}

interface SimpleUser { id: string; email: string }
interface SimpleProduct { id: string; name: string }

export default function AdminLicenses() {
  const [licenses, setLicenses] = useState<AdminLicense[]>([]);
  const [users, setUsers] = useState<SimpleUser[]>([]);
  const [products, setProducts] = useState<SimpleProduct[]>([]);
  const [creating, setCreating] = useState<{ userId?: string; productId?: string; expiresAt?: string }>({});
  const [error, setError] = useState<string | null>(null);

  async function load() {
    try {
      const [l, u, p] = await Promise.all([
        api<AdminLicense[]>("/admin/licenses"),
        api<SimpleUser[]>("/admin/users"),
        api<SimpleProduct[]>("/admin/products"),
      ]);
      setLicenses(l);
      setUsers(u);
      setProducts(p);
    } catch (e: any) {
      setError(e.message);
    }
  }
  useEffect(() => {
    load();
  }, []);

  async function create() {
    if (!creating.userId || !creating.productId) return;
    try {
      await api("/admin/licenses", {
        method: "POST",
        body: JSON.stringify({
          userId: creating.userId,
          productId: creating.productId,
          expiresAt: creating.expiresAt
            ? new Date(creating.expiresAt).toISOString()
            : null,
        }),
      });
      setCreating({});
      await load();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function setStatus(id: string, status: string) {
    await api(`/admin/licenses/${id}`, {
      method: "PATCH",
      body: JSON.stringify({ status }),
    });
    await load();
  }

  async function setMax(id: string, maxActivations: number) {
    if (!Number.isFinite(maxActivations) || maxActivations < 1) return;
    await api(`/admin/licenses/${id}`, {
      method: "PATCH",
      body: JSON.stringify({ maxActivations }),
    });
    await load();
  }

  async function freeDevice(licenseId: string, activationId: string) {
    if (!confirm("Free this device? It will need to re-activate.")) return;
    await api(`/admin/licenses/${licenseId}/activations/${activationId}`, {
      method: "DELETE",
    });
    await load();
  }

  async function sendKey(id: string, email: string) {
    if (!confirm(`Email this license key to ${email}?`)) return;
    try {
      const r = await api<{ sent: boolean }>(`/admin/licenses/${id}/send-key`, {
        method: "POST",
        body: JSON.stringify({}),
      });
      alert(r.sent ? `License key emailed to ${email}.` : "Email is not configured (SMTP) — nothing sent.");
    } catch (e: any) {
      alert(e.message);
    }
  }

  async function remove(id: string) {
    if (!confirm("Delete this license?")) return;
    await api(`/admin/licenses/${id}`, { method: "DELETE" });
    await load();
  }

  return (
    <div>
      {error && <div className="alert error">{error}</div>}
      <div className="card">
        <h3>Issue license</h3>
        <div className="row">
          <select
            value={creating.userId || ""}
            onChange={(e) => setCreating({ ...creating, userId: e.target.value })}
          >
            <option value="">— user —</option>
            {users.map((u) => (
              <option key={u.id} value={u.id}>{u.email}</option>
            ))}
          </select>
          <select
            value={creating.productId || ""}
            onChange={(e) => setCreating({ ...creating, productId: e.target.value })}
          >
            <option value="">— product —</option>
            {products.map((p) => (
              <option key={p.id} value={p.id}>{p.name}</option>
            ))}
          </select>
          <input
            className="input"
            type="date"
            style={{ width: 180 }}
            value={creating.expiresAt || ""}
            onChange={(e) => setCreating({ ...creating, expiresAt: e.target.value })}
          />
          <button className="btn" onClick={create}>Create</button>
        </div>
      </div>

      <table>
        <thead>
          <tr>
            <th>License key</th>
            <th>User</th>
            <th>Product</th>
            <th>Status</th>
            <th>Devices</th>
            <th>Expires</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {licenses.map((l) => (
            <tr key={l.id}>
              <td><code>{l.key}</code></td>
              <td>{l.user.email}</td>
              <td>{l.product.name}</td>
              <td>
                <select value={l.status} onChange={(e) => setStatus(l.id, e.target.value)}>
                  <option value="ACTIVE">ACTIVE</option>
                  <option value="REVOKED">REVOKED</option>
                  <option value="EXPIRED">EXPIRED</option>
                </select>
              </td>
              <td>
                <div className="row" style={{ alignItems: "center", gap: 6 }}>
                  <span>{l.activations.length} /</span>
                  <input
                    className="input"
                    type="number"
                    min={1}
                    defaultValue={l.maxActivations}
                    style={{ width: 64 }}
                    onBlur={(e) => {
                      const n = parseInt(e.target.value, 10);
                      if (n !== l.maxActivations) setMax(l.id, n);
                    }}
                  />
                </div>
                {l.activations.length > 0 && (
                  <details style={{ marginTop: 4 }}>
                    <summary className="muted" style={{ fontSize: "0.8rem" }}>
                      devices
                    </summary>
                    <ul style={{ margin: "4px 0 0", paddingLeft: 16 }}>
                      {l.activations.map((a) => (
                        <li key={a.id} style={{ fontSize: "0.8rem", marginBottom: 2 }}>
                          <code>{a.machineId.slice(0, 12)}…</code>
                          {a.appVersion ? ` · v${a.appVersion}` : ""} ·{" "}
                          {new Date(a.createdAt).toLocaleDateString()}{" "}
                          <button
                            className="btn danger"
                            style={{ fontSize: "0.7rem", padding: "1px 6px" }}
                            onClick={() => freeDevice(l.id, a.id)}
                          >
                            Free
                          </button>
                        </li>
                      ))}
                    </ul>
                  </details>
                )}
              </td>
              <td>{l.expiresAt ? new Date(l.expiresAt).toLocaleDateString() : "—"}</td>
              <td>
                <div className="row">
                  <button className="btn secondary" onClick={() => sendKey(l.id, l.user.email)}>Send key</button>
                  <button className="btn danger" onClick={() => remove(l.id)}>Delete</button>
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
