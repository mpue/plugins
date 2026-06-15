import React, { useEffect, useState } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { api } from "../api";

export function CheckoutSuccess() {
  const [searchParams] = useSearchParams();
  const sessionId = searchParams.get("session_id");
  const [status, setStatus] = useState<"pending" | "ok" | "error">("pending");
  const [errorMsg, setErrorMsg] = useState<string | null>(null);

  useEffect(() => {
    if (!sessionId) {
      setStatus("ok");
      return;
    }
    api(`/checkout/verify?session_id=${encodeURIComponent(sessionId)}`, { method: "POST" })
      .then(() => setStatus("ok"))
      .catch((e: any) => {
        setErrorMsg(e.message);
        setStatus("error");
      });
  }, [sessionId]);

  if (status === "pending") {
    return <div className="container"><p className="muted">Activating your licenses…</p></div>;
  }

  return (
    <div className="container">
      {status === "error" ? (
        <div className="alert error">
          License activation failed: {errorMsg}.<br />
          Your payment was received — please contact support if licenses don't appear in your profile.
        </div>
      ) : (
        <div className="alert success">Payment received. Your licenses have been activated!</div>
      )}
      <Link to="/profile" className="btn">Go to profile</Link>
    </div>
  );
}

export function CheckoutCancel() {
  return (
    <div className="container">
      <div className="alert error">Checkout was cancelled.</div>
      <Link to="/shop" className="btn">Back to shop</Link>
    </div>
  );
}
