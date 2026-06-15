import React, { useEffect, useState } from "react";
import { api } from "../../api";

interface ConfigItem { key: string; value: string }

export default function AdminConfig() {
  const [items, setItems] = useState<ConfigItem[]>([]);
  const [draft, setDraft] = useState<Record<string, string>>({});
  const [newKey, setNewKey] = useState("");
  const [newValue, setNewValue] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  async function load() {
    try {
      const data = await api<ConfigItem[]>("/admin/config");
      setItems(data);
      const d: Record<string, string> = {};
      for (const i of data) d[i.key] = i.value;
      setDraft(d);
    } catch (e: any) {
      setError(e.message);
    }
  }
  useEffect(() => { load(); }, []);

  async function save(key: string) {
    setError(null); setSuccess(null);
    try {
      await api(`/admin/config/${encodeURIComponent(key)}`, {
        method: "PUT",
        body: JSON.stringify({ value: draft[key] ?? "" }),
      });
      setSuccess(`Saved ${key}`);
      load();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function add() {
    if (!newKey) return;
    await api(`/admin/config/${encodeURIComponent(newKey)}`, {
      method: "PUT",
      body: JSON.stringify({ value: newValue }),
    });
    setNewKey(""); setNewValue("");
    await load();
  }

  async function remove(key: string) {
    if (!confirm(`Delete config ${key}?`)) return;
    await api(`/admin/config/${encodeURIComponent(key)}`, { method: "DELETE" });
    await load();
  }

  return (
    <div>
      {error && <div className="alert error">{error}</div>}
      {success && <div className="alert success">{success}</div>}

      <h3>Add config entry</h3>
      <div className="card">
        <div className="row">
          <input className="input" placeholder="key (e.g. site.title)" value={newKey} onChange={(e) => setNewKey(e.target.value)} style={{ flex: 1 }} />
          <input className="input" placeholder="value" value={newValue} onChange={(e) => setNewValue(e.target.value)} style={{ flex: 2 }} />
          <button className="btn" onClick={add}>Add</button>
        </div>
      </div>

      <h3>Existing</h3>
      {items.map((item) => (
        <div className="card" key={item.key}>
          <label>{item.key}</label>
          <textarea
            className="textarea"
            value={draft[item.key] ?? ""}
            onChange={(e) => setDraft({ ...draft, [item.key]: e.target.value })}
          />
          <div className="row" style={{ marginTop: 8 }}>
            <button className="btn" onClick={() => save(item.key)}>Save</button>
            <button className="btn danger" onClick={() => remove(item.key)}>Delete</button>
          </div>
        </div>
      ))}
    </div>
  );
}
