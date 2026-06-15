import React from "react";
import { NavLink, useNavigate } from "react-router-dom";
import { useAuth } from "../auth";

export default function Navbar({ siteTitle }: { siteTitle: string }) {
  const { user, logout } = useAuth();
  const navigate = useNavigate();
  return (
    <nav className="navbar">
      <div className="brand">{siteTitle}</div>
      <NavLink to="/" end>Home</NavLink>
      <NavLink to="/shop">Shop</NavLink>
      {user && <NavLink to="/profile">Profile</NavLink>}
      {user?.role === "ADMIN" && <NavLink to="/admin/products">Admin</NavLink>}
      <div className="spacer" />
      {user ? (
        <>
          <span className="muted">{user.email}</span>
          <button
            className="btn secondary"
            onClick={() => {
              logout();
              navigate("/");
            }}
          >
            Logout
          </button>
        </>
      ) : (
        <>
          <NavLink to="/login">Login</NavLink>
          <NavLink to="/register">Register</NavLink>
        </>
      )}
    </nav>
  );
}
