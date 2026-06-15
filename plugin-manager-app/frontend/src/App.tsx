import React, { useEffect, useState } from "react";
import { BrowserRouter, Navigate, Route, Routes, useLocation } from "react-router-dom";
import { AuthProvider } from "./auth";
import Navbar from "./components/Navbar";
import { ProtectedRoute } from "./components/ProtectedRoute";
import Landing from "./pages/landing/Landing";
import Shop from "./pages/Shop";
import ProductPage from "./pages/Product";
import Login from "./pages/Login";
import Register from "./pages/Register";
import ForgotPassword from "./pages/ForgotPassword";
import ResetPassword from "./pages/ResetPassword";
import Profile from "./pages/Profile";
import { CheckoutCancel, CheckoutSuccess } from "./pages/CheckoutResult";
import AdminLayout from "./pages/admin/AdminLayout";
import AdminProducts from "./pages/admin/Products";
import AdminUsers from "./pages/admin/Users";
import AdminLicenses from "./pages/admin/Licenses";
import AdminConfig from "./pages/admin/Config";
import AdminBackup from "./pages/admin/Backup";
import { api } from "./api";

// The landing route is a full-screen immersive page with its own nav — hide the
// app chrome (Navbar) there.
function ChromeNavbar({ siteTitle }: { siteTitle: string }) {
  const { pathname } = useLocation();
  if (pathname === "/") return null;
  return <Navbar siteTitle={siteTitle} />;
}

export default function App() {
  const [siteTitle, setSiteTitle] = useState("Plugin Manager");

  useEffect(() => {
    api<Record<string, string>>("/config/public")
      .then((cfg) => {
        if (cfg["site.title"]) {
          setSiteTitle(cfg["site.title"]);
          document.title = cfg["site.title"];
        }
      })
      .catch(() => {});
  }, []);

  return (
    <BrowserRouter>
      <AuthProvider>
        <ChromeNavbar siteTitle={siteTitle} />
        <Routes>
          <Route path="/" element={<Landing />} />
          <Route path="/shop" element={<Shop />} />
          <Route path="/product/:slug" element={<ProductPage />} />
          <Route path="/login" element={<Login />} />
          <Route path="/register" element={<Register />} />
          <Route path="/forgot-password" element={<ForgotPassword />} />
          <Route path="/reset-password" element={<ResetPassword />} />
          <Route path="/checkout/success" element={<CheckoutSuccess />} />
          <Route path="/checkout/cancel" element={<CheckoutCancel />} />
          <Route
            path="/profile"
            element={
              <ProtectedRoute>
                <Profile />
              </ProtectedRoute>
            }
          />
          <Route
            path="/admin"
            element={
              <ProtectedRoute adminOnly>
                <AdminLayout />
              </ProtectedRoute>
            }
          >
            <Route index element={<Navigate to="products" replace />} />
            <Route path="products" element={<AdminProducts />} />
            <Route path="users" element={<AdminUsers />} />
            <Route path="licenses" element={<AdminLicenses />} />
            <Route path="config" element={<AdminConfig />} />
            <Route path="backup" element={<AdminBackup />} />
          </Route>
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </AuthProvider>
    </BrowserRouter>
  );
}
