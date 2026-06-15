import dotenv from "dotenv";
dotenv.config();

export const config = {
  port: Number(process.env.PORT || 4000),
  jwtSecret: process.env.JWT_SECRET || "dev-secret",
  jwtExpiresIn: process.env.JWT_EXPIRES_IN || "7d",
  frontendUrl: process.env.FRONTEND_URL || "http://localhost:8080",
  stripe: {
    secretKey: process.env.STRIPE_SECRET_KEY || "",
    webhookSecret: process.env.STRIPE_WEBHOOK_SECRET || "",
    publishableKey: process.env.STRIPE_PUBLISHABLE_KEY || "",
  },
  uploadDir: process.env.UPLOAD_DIR || "./data/uploads",
  productFileDir: process.env.PRODUCT_FILE_DIR || "./data/products",
  downloadTokenTtlSeconds: Number(process.env.DOWNLOAD_TOKEN_TTL_SECONDS || 300),
  adminEmail: process.env.ADMIN_EMAIL || "admin@example.com",
  adminPassword: process.env.ADMIN_PASSWORD || "admin12345",
  // Ed25519 private key (PKCS8 PEM) for signing activation tokens. Stored in env
  // as a single line with \n escapes; un-escaped here. Generate with
  // `node scripts/generate-license-keypair.js`. KEEP SECRET.
  licenseSigningPrivateKey: (process.env.LICENSE_SIGNING_PRIVATE_KEY || "").replace(/\\n/g, "\n"),
  // Default seat limit applied when a license has no explicit maxActivations.
  licenseDefaultMaxActivations: Number(process.env.LICENSE_MAX_ACTIVATIONS || 3),
  // Public base URL of the web app, used to build links in emails (reset/login).
  appBaseUrl: process.env.APP_BASE_URL || process.env.FRONTEND_URL || "http://localhost:8080",
  // Outgoing email (SMTP). If host is empty, email is disabled (calls are logged
  // and skipped, so the app still works without a mail server).
  mail: {
    host: process.env.SMTP_HOST || "",
    port: Number(process.env.SMTP_PORT || 587),
    secure: process.env.SMTP_SECURE === "true",
    user: process.env.SMTP_USER || "",
    pass: process.env.SMTP_PASS || "",
    from: process.env.SMTP_FROM || "Pueski Audio <noreply@pueski.de>",
  },
  passwordResetTtlMinutes: Number(process.env.PASSWORD_RESET_TTL_MINUTES || 60),
};
