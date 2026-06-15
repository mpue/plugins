import React from "react";
import { NavLink, Outlet } from "react-router-dom";

export default function AdminLayout() {
  return (
    <div className="container">
      <h1>Admin</h1>
      <nav className="admin-nav">
        <NavLink to="/admin/products">Products</NavLink>
        <NavLink to="/admin/users">Users</NavLink>
        <NavLink to="/admin/licenses">Licenses</NavLink>
        <NavLink to="/admin/config">Configuration</NavLink>
        <NavLink to="/admin/backup">Backup</NavLink>
      </nav>
      <Outlet />
    </div>
  );
}
