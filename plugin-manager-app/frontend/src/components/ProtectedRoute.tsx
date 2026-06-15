import React from "react";
import { Navigate, useLocation } from "react-router-dom";
import { useAuth } from "../auth";

export function ProtectedRoute({
  children,
  adminOnly = false,
}: {
  children: React.ReactNode;
  adminOnly?: boolean;
}) {
  const { user, loading } = useAuth();
  const location = useLocation();
  if (loading) return <div className="container">Loading…</div>;
  if (!user) return <Navigate to="/login" replace state={{ from: location.pathname }} />;
  if (adminOnly && user.role !== "ADMIN") return <Navigate to="/" replace />;
  return <>{children}</>;
}
