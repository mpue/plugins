import { Router } from "express";
import fs from "fs";
import path from "path";
import { z } from "zod";
import bcrypt from "bcryptjs";
import archiver from "archiver";
import multer from "multer";
import unzipper from "unzipper";
import { prisma } from "../db";
import { adminRequired, authRequired } from "../middleware/auth";
import { uploadImage, uploadProductFile, imagePublicUrl, productFilePath, deleteFileSafe } from "../services/storage";
import { LicenseStatus, Platform, PluginFormat, Role } from "@prisma/client";
import { generateLicenseKey } from "../services/license";
import { config } from "../config";
import { sendLicenseKeyEmail, sendPasswordResetEmail } from "../services/mailer";
import { createPasswordResetToken, resetLink } from "../services/password-reset";

const router = Router();
router.use(authRequired, adminRequired);

// ----- Users -----
router.get("/users", async (_req, res) => {
  const users = await prisma.user.findMany({
    orderBy: { createdAt: "desc" },
    select: {
      id: true, email: true, name: true, role: true, createdAt: true,
      _count: { select: { licenses: true, orders: true } },
    },
  });
  res.json(users);
});

const updateUserSchema = z.object({
  name: z.string().nullable().optional(),
  role: z.nativeEnum(Role).optional(),
  password: z.string().min(8).optional(),
});

router.patch("/users/:id", async (req, res) => {
  const parsed = updateUserSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });
  const data: any = { ...parsed.data };
  if (parsed.data.password) {
    data.passwordHash = await bcrypt.hash(parsed.data.password, 10);
    delete data.password;
  }
  const user = await prisma.user.update({ where: { id: req.params.id }, data });
  res.json({ id: user.id, email: user.email, name: user.name, role: user.role });
});

router.delete("/users/:id", async (req, res) => {
  if (req.params.id === req.user!.id) {
    return res.status(400).json({ error: "Cannot delete yourself" });
  }
  await prisma.user.delete({ where: { id: req.params.id } });
  res.json({ ok: true });
});

// ----- Products -----
const productSchema = z.object({
  slug: z.string().min(1).max(100),
  name: z.string().min(1),
  description: z.string().min(1),
  priceCents: z.number().int().min(0),
  currency: z.string().length(3).default("EUR"),
  version: z.string().default("1.0.0"),
  active: z.boolean().default(true),
});

router.get("/products", async (_req, res) => {
  const products = await prisma.product.findMany({
    orderBy: { createdAt: "desc" },
    include: { _count: { select: { licenses: true } }, productFiles: { orderBy: { createdAt: "asc" } } },
  });
  res.json(products);
});

router.post("/products", async (req, res) => {
  const parsed = productSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input", details: parsed.error.flatten() });
  try {
    const p = await prisma.product.create({ data: parsed.data });
    res.status(201).json(p);
  } catch (e: any) {
    if (e.code === "P2002") return res.status(409).json({ error: "Slug already exists" });
    throw e;
  }
});

router.patch("/products/:id", async (req, res) => {
  const parsed = productSchema.partial().safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });
  const p = await prisma.product.update({
    where: { id: req.params.id },
    data: parsed.data,
  });
  res.json(p);
});

router.delete("/products/:id", async (req, res) => {
  const p = await prisma.product.findUnique({ where: { id: req.params.id } });
  if (!p) return res.status(404).json({ error: "Not found" });
  await prisma.product.delete({ where: { id: req.params.id } });
  const files = await prisma.productFile.findMany({ where: { productId: req.params.id } });
  for (const f of files) deleteFileSafe(productFilePath(f.fileName));
  res.json({ ok: true });
});

router.post("/products/:id/image", uploadImage.single("file"), async (req, res) => {
  if (!req.file) return res.status(400).json({ error: "No file" });
  const p = await prisma.product.update({
    where: { id: req.params.id },
    data: { imageUrl: imagePublicUrl(req.file.filename) },
  });
  res.json(p);
});

const productFileMetaSchema = z.object({
  platform: z.nativeEnum(Platform),
  format: z.nativeEnum(PluginFormat),
});

router.get("/products/:id/files", async (req, res) => {
  const files = await prisma.productFile.findMany({
    where: { productId: req.params.id },
    orderBy: { createdAt: "asc" },
  });
  res.json(files);
});

router.post("/products/:id/files", uploadProductFile.single("file"), async (req, res) => {
  if (!req.file) return res.status(400).json({ error: "No file" });
  const parsed = productFileMetaSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "platform and format are required", details: parsed.error.flatten() });

  // Replace existing file for same platform+format combination
  const existing = await prisma.productFile.findFirst({
    where: { productId: req.params.id, platform: parsed.data.platform, format: parsed.data.format },
  });
  if (existing) {
    deleteFileSafe(productFilePath(existing.fileName));
    await prisma.productFile.delete({ where: { id: existing.id } });
  }

  const pf = await prisma.productFile.create({
    data: {
      productId: req.params.id,
      platform: parsed.data.platform,
      format: parsed.data.format,
      fileName: req.file.filename,
      fileSize: req.file.size,
    },
  });
  res.status(201).json(pf);
});

router.delete("/products/:id/files/:fileId", async (req, res) => {
  const pf = await prisma.productFile.findFirst({
    where: { id: req.params.fileId, productId: req.params.id },
  });
  if (!pf) return res.status(404).json({ error: "Not found" });
  deleteFileSafe(productFilePath(pf.fileName));
  await prisma.productFile.delete({ where: { id: pf.id } });
  res.json({ ok: true });
});

// ----- Licenses -----
router.get("/licenses", async (_req, res) => {
  const licenses = await prisma.license.findMany({
    orderBy: { createdAt: "desc" },
    include: {
      user: { select: { id: true, email: true } },
      product: { select: { id: true, name: true, slug: true } },
      activations: {
        select: { id: true, machineId: true, appVersion: true, createdAt: true },
        orderBy: { createdAt: "asc" },
      },
    },
  });
  res.json(licenses);
});

const createLicenseSchema = z.object({
  userId: z.string().uuid(),
  productId: z.string().uuid(),
  expiresAt: z.string().datetime().nullable().optional(),
});

router.post("/licenses", async (req, res) => {
  const parsed = createLicenseSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });
  const license = await prisma.license.create({
    data: {
      key: generateLicenseKey(),
      userId: parsed.data.userId,
      productId: parsed.data.productId,
      expiresAt: parsed.data.expiresAt ? new Date(parsed.data.expiresAt) : null,
    },
    include: { user: true, product: true },
  });

  // Email the key to the licensee — best-effort.
  sendLicenseKeyEmail(license.user.email, license.product.name, license.key).catch((e) =>
    console.error("license key email failed:", e)
  );

  res.status(201).json(license);
});

// Resend the license-key email for an existing license.
router.post("/licenses/:id/send-key", async (req, res) => {
  const license = await prisma.license.findUnique({
    where: { id: req.params.id },
    include: { user: true, product: true },
  });
  if (!license) return res.status(404).json({ error: "Not found" });
  const sent = await sendLicenseKeyEmail(license.user.email, license.product.name, license.key);
  res.json({ ok: true, sent, to: license.user.email });
});

// Send a password-reset / set-password link to a user (admin-triggered).
router.post("/users/:id/send-reset", async (req, res) => {
  const user = await prisma.user.findUnique({ where: { id: req.params.id } });
  if (!user) return res.status(404).json({ error: "Not found" });
  const raw = await createPasswordResetToken(user.id);
  const isNew = req.body?.isNewAccount === true;
  const sent = await sendPasswordResetEmail(user.email, resetLink(raw), isNew);
  res.json({ ok: true, sent, to: user.email });
});

router.patch("/licenses/:id", async (req, res) => {
  const schema = z.object({
    status: z.nativeEnum(LicenseStatus).optional(),
    expiresAt: z.string().datetime().nullable().optional(),
    maxActivations: z.number().int().min(1).max(1000).optional(),
  });
  const parsed = schema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });
  const license = await prisma.license.update({
    where: { id: req.params.id },
    data: {
      status: parsed.data.status,
      maxActivations: parsed.data.maxActivations,
      expiresAt:
        parsed.data.expiresAt === undefined
          ? undefined
          : parsed.data.expiresAt
          ? new Date(parsed.data.expiresAt)
          : null,
    },
  });
  res.json(license);
});

// Free a single device/seat (admin support action).
router.delete("/licenses/:id/activations/:activationId", async (req, res) => {
  await prisma.activation.deleteMany({
    where: { id: req.params.activationId, licenseId: req.params.id },
  });
  res.json({ ok: true });
});

router.delete("/licenses/:id", async (req, res) => {
  await prisma.license.delete({ where: { id: req.params.id } });
  res.json({ ok: true });
});

// ----- System config -----
router.get("/config", async (_req, res) => {
  const items = await prisma.systemConfig.findMany();
  res.json(items);
});

router.put("/config/:key", async (req, res) => {
  const schema = z.object({ value: z.string() });
  const parsed = schema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });
  const item = await prisma.systemConfig.upsert({
    where: { key: req.params.key },
    create: { key: req.params.key, value: parsed.data.value },
    update: { value: parsed.data.value },
  });
  res.json(item);
});

router.delete("/config/:key", async (req, res) => {
  await prisma.systemConfig.delete({ where: { key: req.params.key } });
  res.json({ ok: true });
});

// ----- Backup -----
router.get("/backup", async (_req, res) => {
  try {
    const [users, products, productFiles, orders, orderItems, licenses, systemConfigs] = await Promise.all([
      prisma.user.findMany(),
      prisma.product.findMany(),
      prisma.productFile.findMany(),
      prisma.order.findMany(),
      prisma.orderItem.findMany(),
      prisma.license.findMany(),
      prisma.systemConfig.findMany(),
    ]);

    const dbData = JSON.stringify({ users, products, productFiles, orders, orderItems, licenses, systemConfigs }, null, 2);

    const timestamp = new Date().toISOString().replace(/[:.]/g, "-");
    res.setHeader("Content-Type", "application/zip");
    res.setHeader("Content-Disposition", `attachment; filename="backup-${timestamp}.zip"`);

    const archive = archiver("zip", { zlib: { level: 6 } });
    archive.on("error", (err) => { throw err; });
    archive.pipe(res);

    archive.append(dbData, { name: "db.json" });

    const uploadsDir = config.uploadDir;
    if (fs.existsSync(uploadsDir)) {
      archive.directory(uploadsDir, "uploads");
    }

    const productsDir = config.productFileDir;
    if (fs.existsSync(productsDir)) {
      archive.directory(productsDir, "products");
    }

    await archive.finalize();
  } catch (err: any) {
    res.status(500).json({ error: err.message || "Backup failed" });
  }
});

// ----- Restore -----
const restoreUpload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: 4 * 1024 * 1024 * 1024 },
});

function clearDirectory(dir: string) {
  fs.mkdirSync(dir, { recursive: true });
  for (const entry of fs.readdirSync(dir)) {
    fs.rmSync(path.join(dir, entry), { recursive: true, force: true });
  }
}

function toDateOrUndefined(value: any) {
  if (value === null || value === undefined) return undefined;
  return new Date(value);
}

router.post("/restore", restoreUpload.single("backup"), async (req, res) => {
  if (!req.file) return res.status(400).json({ error: "No backup file provided" });

  try {
    const zipBuffer = req.file.buffer;
    const directory = await unzipper.Open.buffer(zipBuffer);

    // Read db.json
    const dbEntry = directory.files.find((f) => f.path === "db.json");
    if (!dbEntry) return res.status(400).json({ error: "Invalid backup: db.json missing" });

    const dbContent = await dbEntry.buffer();
    const db = JSON.parse(dbContent.toString("utf-8"));

    const users = Array.isArray(db.users) ? db.users : [];
    const productsRaw = Array.isArray(db.products) ? db.products : [];
    const orders = Array.isArray(db.orders) ? db.orders : [];
    const orderItems = Array.isArray(db.orderItems) ? db.orderItems : [];
    const licenses = Array.isArray(db.licenses) ? db.licenses : [];
    const systemConfigs = Array.isArray(db.systemConfigs) ? db.systemConfigs : [];

    // Legacy compatibility: older backups stored fileName/fileSize on product directly.
    const products = productsRaw.map((p: any) => {
      const { fileName: _legacyFileName, fileSize: _legacyFileSize, ...rest } = p || {};
      return rest;
    });

    const productFiles = Array.isArray(db.productFiles)
      ? db.productFiles
      : productsRaw
          .filter((p: any) => p?.id && p?.fileName)
          .map((p: any) => ({
            id: crypto.randomUUID(),
            productId: p.id,
            platform: "MAC",
            format: "VST3",
            fileName: p.fileName,
            fileSize: Number(p.fileSize || 0),
            createdAt: toDateOrUndefined(p.createdAt) || new Date(),
          }));

    clearDirectory(config.uploadDir);
    clearDirectory(config.productFileDir);

    // Restore files: uploads + products
    for (const file of directory.files) {
      if (file.type === "Directory") continue;

      let destDir: string | null = null;
      let relPath: string | null = null;

      if (file.path.startsWith("uploads/")) {
        destDir = config.uploadDir;
        relPath = file.path.slice("uploads/".length);
      } else if (file.path.startsWith("products/")) {
        destDir = config.productFileDir;
        relPath = file.path.slice("products/".length);
      }

      if (destDir && relPath) {
        const resolvedDestDir = path.resolve(destDir);
        const destPath = path.resolve(destDir, relPath);
        // Prevent path traversal
        if (!destPath.startsWith(resolvedDestDir + path.sep) && destPath !== resolvedDestDir) {
          continue;
        }
        fs.mkdirSync(path.dirname(destPath), { recursive: true });
        const fileBuffer = await file.buffer();
        fs.writeFileSync(destPath, fileBuffer);
      }
    }

    // Restore DB in a transaction: delete all existing data, then re-insert
    await prisma.$transaction(async (tx) => {
      // Delete in dependency order
      await tx.license.deleteMany();
      await tx.orderItem.deleteMany();
      await tx.order.deleteMany();
      await tx.productFile.deleteMany();
      await tx.product.deleteMany();
      await tx.user.deleteMany();
      await tx.systemConfig.deleteMany();

      // Re-insert
      if (users.length) await tx.user.createMany({ data: users });
      if (products.length) await tx.product.createMany({ data: products });
      if (productFiles.length) await tx.productFile.createMany({ data: productFiles });
      if (orders.length) await tx.order.createMany({ data: orders });
      if (orderItems.length) await tx.orderItem.createMany({ data: orderItems });
      if (licenses.length) await tx.license.createMany({ data: licenses });
      if (systemConfigs.length) await tx.systemConfig.createMany({ data: systemConfigs });
    });

    res.json({ ok: true, message: "Backup successfully restored" });
  } catch (err: any) {
    res.status(500).json({ error: err.message || "Restore failed" });
  }
});

export default router;
