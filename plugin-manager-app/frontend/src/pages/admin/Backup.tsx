import React, { useRef, useState } from "react";
import { getToken } from "../../api";

const API_BASE = process.env.API_BASE_URL || "";

export default function AdminBackup() {
  const [restoreFile, setRestoreFile] = useState<File | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  function handleDownload() {
    const token = getToken();
    const link = document.createElement("a");
    link.href = `${API_BASE}/api/admin/backup`;
    link.setAttribute("download", "");
    // Add auth header via fetch and blob
    setLoading(true);
    setError(null);
    fetch(`${API_BASE}/api/admin/backup`, {
      headers: { Authorization: `Bearer ${token}` },
    })
      .then(async (res) => {
        if (!res.ok) {
          const body = await res.json().catch(() => ({}));
          throw new Error(body.error || `HTTP ${res.status}`);
        }
        const disposition = res.headers.get("Content-Disposition") || "";
        const match = disposition.match(/filename="([^"]+)"/);
        const filename = match ? match[1] : "backup.zip";
        return res.blob().then((blob) => ({ blob, filename }));
      })
      .then(({ blob, filename }) => {
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
        setSuccess("Backup erfolgreich heruntergeladen.");
      })
      .catch((e: any) => setError(e.message))
      .finally(() => setLoading(false));
  }

  async function handleRestore(e: React.FormEvent) {
    e.preventDefault();
    if (!restoreFile) return;
    if (
      !window.confirm(
        "ACHTUNG: Das Wiederherstellen überschreibt alle vorhandenen Daten. Fortfahren?"
      )
    )
      return;

    setLoading(true);
    setError(null);
    setSuccess(null);

    const fd = new FormData();
    fd.append("backup", restoreFile);

    const token = getToken();
    try {
      const res = await fetch(`${API_BASE}/api/admin/restore`, {
        method: "POST",
        headers: { Authorization: `Bearer ${token}` },
        body: fd,
      });
      const data = await res.json().catch(() => ({}));
      if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
      setSuccess(data.message || "Backup erfolgreich wiederhergestellt.");
      setRestoreFile(null);
      if (fileInputRef.current) fileInputRef.current.value = "";
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  return (
    <div>
      <h2>Backup &amp; Restore</h2>

      {error && <div className="alert error">{error}</div>}
      {success && <div className="alert success">{success}</div>}

      <div className="card" style={{ marginBottom: 24 }}>
        <h3>Backup erstellen</h3>
        <p>
          Erstellt ein ZIP-Archiv mit der Datenbank (alle Nutzer, Produkte,
          Bestellungen, Lizenzen, Konfiguration) sowie allen hochgeladenen
          Bildern und Produktdateien.
        </p>
        <button className="btn" onClick={handleDownload} disabled={loading}>
          {loading ? "Erstelle Backup…" : "Backup herunterladen"}
        </button>
      </div>

      <div className="card">
        <h3>Backup wiederherstellen</h3>
        <p style={{ color: "#c00" }}>
          <strong>Warnung:</strong> Alle vorhandenen Daten werden gelöscht und
          durch den Backup-Stand ersetzt.
        </p>
        <form onSubmit={handleRestore}>
          <div className="row" style={{ alignItems: "center", gap: 12 }}>
            <input
              ref={fileInputRef}
              type="file"
              accept=".zip"
              className="input"
              onChange={(e) => setRestoreFile(e.target.files?.[0] ?? null)}
              disabled={loading}
            />
            <button
              className="btn btn-danger"
              type="submit"
              disabled={!restoreFile || loading}
            >
              {loading ? "Wird wiederhergestellt…" : "Wiederherstellen"}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
