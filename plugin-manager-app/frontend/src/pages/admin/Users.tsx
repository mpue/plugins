import React, { useEffect, useState } from "react";
import { api } from "../../api";

interface AdminUser {
  id: string;
  email: string;
  name: string | null;
  role: "USER" | "ADMIN";
  createdAt: string;
  _count: { licenses: number; orders: number };
}

export default function AdminUsers() {
  const [users, setUsers] = useState<AdminUser[]>([]);
  const [error, setError] = useState<string | null>(null);

  async function load() {
    try {
      setUsers(await api<AdminUser[]>("/admin/users"));
    } catch (e: any) {
      setError(e.message);
    }
  }

  useEffect(() => {
    load();
  }, []);

  async function changeRole(id: string, role: "USER" | "ADMIN") {
    await api(`/admin/users/${id}`, {
      method: "PATCH",
      body: JSON.stringify({ role }),
    });
    await load();
  }

  async function resetPassword(id: string) {
    const pwd = prompt("Enter new password (min 8 chars):");
    if (!pwd) return;
    if (pwd.length < 8) return alert("Too short");
    await api(`/admin/users/${id}`, {
      method: "PATCH",
      body: JSON.stringify({ password: pwd }),
    });
    alert("Password updated.");
  }

  async function sendResetEmail(id: string, email: string) {
    if (!confirm(`Send a password-reset email to ${email}?`)) return;
    try {
      const r = await api<{ sent: boolean }>(`/admin/users/${id}/send-reset`, {
        method: "POST",
        body: JSON.stringify({}),
      });
      alert(r.sent ? `Reset email sent to ${email}.` : "Email is not configured (SMTP) — nothing sent.");
    } catch (e: any) {
      alert(e.message);
    }
  }

  async function remove(id: string) {
    if (!confirm("Delete user (and all their data)?")) return;
    await api(`/admin/users/${id}`, { method: "DELETE" });
    await load();
  }

  return (
    <div>
      {error && <div className="alert error">{error}</div>}
      <table>
        <thead>
          <tr>
            <th>Email</th>
            <th>Name</th>
            <th>Role</th>
            <th>Orders</th>
            <th>Licenses</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {users.map((u) => (
            <tr key={u.id}>
              <td>{u.email}</td>
              <td>{u.name || "—"}</td>
              <td>
                <select
                  value={u.role}
                  onChange={(e) => changeRole(u.id, e.target.value as any)}
                >
                  <option value="USER">USER</option>
                  <option value="ADMIN">ADMIN</option>
                </select>
              </td>
              <td>{u._count.orders}</td>
              <td>{u._count.licenses}</td>
              <td>
                <div className="row">
                  <button className="btn secondary" onClick={() => resetPassword(u.id)}>
                    Set password
                  </button>
                  <button className="btn secondary" onClick={() => sendResetEmail(u.id, u.email)}>
                    Send reset email
                  </button>
                  <button className="btn danger" onClick={() => remove(u.id)}>
                    Delete
                  </button>
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
