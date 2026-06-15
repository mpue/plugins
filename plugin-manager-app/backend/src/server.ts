import express from "express";
import cors from "cors";
import helmet from "helmet";
import morgan from "morgan";
import path from "path";
import { config } from "./config";
import "./types";

import authRoutes from "./routes/auth";
import productRoutes from "./routes/products";
import checkoutRoutes from "./routes/checkout";
import webhookRoutes from "./routes/webhooks";
import orderRoutes from "./routes/orders";
import licenseRoutes from "./routes/licenses";
import activationRoutes from "./routes/activation";
import downloadRoutes from "./routes/downloads";
import adminRoutes from "./routes/admin";
import configRoutes from "./routes/config";
import installerRoutes from "./routes/installer";
import { errorHandler, notFound } from "./middleware/error";

export function createApp() {
  const app = express();

  app.use(helmet({ crossOriginResourcePolicy: { policy: "cross-origin" } }));
  app.use(
    cors({
      origin: config.frontendUrl === "*" ? true : config.frontendUrl,
      credentials: true,
    })
  );
  app.use(morgan(process.env.NODE_ENV === "production" ? "combined" : "dev"));

  // Stripe webhook needs the RAW body, mount before json parser.
  app.use("/api/webhooks", webhookRoutes);

  app.use(express.json({ limit: "1mb" }));

  // Static uploads (product images)
  app.use("/uploads", express.static(config.uploadDir));

  app.get("/api/health", (_req, res) => res.json({ ok: true }));
  app.get("/api/public/stripe-key", (_req, res) =>
    res.json({ publishableKey: config.stripe.publishableKey })
  );

  app.use("/api/auth", authRoutes);
  app.use("/api/products", productRoutes);
  app.use("/api/checkout", checkoutRoutes);
  app.use("/api/orders", orderRoutes);
  app.use("/api/licenses", licenseRoutes);
  app.use("/api/activation", activationRoutes);
  app.use("/api/downloads", downloadRoutes);
  app.use("/api/admin", adminRoutes);
  app.use("/api/config", configRoutes);
  app.use("/api/installer", installerRoutes);

  app.use(notFound);
  app.use(errorHandler);

  return app;
}
