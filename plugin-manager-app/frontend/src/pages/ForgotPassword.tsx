import React, { useState } from "react";
import { Link } from "react-router-dom";
import { api } from "../api";

export default function ForgotPassword() {
  const [email, setEmail] = useState("");
  const [busy, setBusy] = useState(false);
  const [done, setDone] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setError(null);
    setBusy(true);
    try {
      await api("/auth/forgot-password", {
        method: "POST",
        body: JSON.stringify({ email }),
      });
      setDone(true);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="container" style={{ maxWidth: 420 }}>
      <h1>Reset password</h1>
      {done ? (
        <div className="card">
          <p>
            If an account exists for <strong>{email}</strong>, a password-reset
            link has been sent. Please check your inbox.
          </p>
          <p className="muted">
            <Link to="/login">Back to login</Link>
          </p>
        </div>
      ) : (
        <>
          {error && <div className="alert error">{error}</div>}
          <form onSubmit={submit} className="card">
            <p className="muted" style={{ marginTop: 0 }}>
              Enter your email and we'll send you a link to set a new password.
            </p>
            <div className="field">
              <label>Email</label>
              <input
                className="input"
                type="email"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                required
              />
            </div>
            <button className="btn" disabled={busy}>
              {busy ? "..." : "Send reset link"}
            </button>
          </form>
          <p className="muted">
            <Link to="/login">Back to login</Link>
          </p>
        </>
      )}
    </div>
  );
}
