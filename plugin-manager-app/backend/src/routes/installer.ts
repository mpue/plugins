import { Router } from "express";
import fs from "fs";
import path from "path";
import { prisma } from "../db";
import { apiTokenAuth } from "../middleware/auth";
import { productFilePath } from "../services/storage";
import { LicenseStatus } from "@prisma/client";

const router = Router();
router.use(apiTokenAuth);

router.get("/me", async (req, res) => {
  const user = await prisma.user.findUnique({ where: { id: req.user!.id } });
  if (!user) return res.status(404).json({ error: "Not found" });
  res.json({ id: user.id, email: user.email, name: user.name });
});

router.get("/products", async (req, res) => {
  const licenses = await prisma.license.findMany({
    where: { userId: req.user!.id, status: LicenseStatus.ACTIVE },
    include: { product: { include: { productFiles: { orderBy: { createdAt: "asc" } } } } },
    orderBy: { createdAt: "desc" },
  });

  const items = licenses.map((l) => ({
    licenseId: l.id,
    licenseKey: l.key,
    status: l.status,
    expiresAt: l.expiresAt,
    product: {
      id: l.product.id,
      slug: l.product.slug,
      name: l.product.name,
      version: l.product.version,
      files: l.product.productFiles.map((f) => ({
        id: f.id,
        platform: f.platform,
        format: f.format,
        fileSize: f.fileSize,
      })),
      hasFile: l.product.productFiles.length > 0,
    },
  }));
  res.json(items);
});

router.get("/products/:licenseId/download", async (req, res) => {
  const license = await prisma.license.findFirst({
    where: { id: req.params.licenseId, userId: req.user!.id },
    include: { product: { include: { productFiles: { orderBy: { createdAt: "asc" } } } } },
  });
  if (!license) return res.status(404).json({ error: "License not found" });
  if (license.status !== LicenseStatus.ACTIVE) {
    return res.status(403).json({ error: "License is not active" });
  }

  const fileId = (req.query.fileId as string) || "";
  const pf = fileId
    ? license.product.productFiles.find((f) => f.id === fileId)
    : license.product.productFiles[0];

  if (!pf) {
    return res.status(404).json({ error: "Product has no downloadable file" });
  }

  const abs = productFilePath(pf.fileName);
  if (!fs.existsSync(abs)) return res.status(404).json({ error: "File missing" });

  const safeName = `${license.product.slug}-${license.product.version}-${pf.platform.toLowerCase()}-${pf.format.toLowerCase()}${path.extname(abs)}`;
  res.setHeader("Content-Disposition", `attachment; filename="${safeName}"`);
  res.setHeader("Content-Type", "application/octet-stream");
  fs.createReadStream(abs).pipe(res);
});

router.post("/license/validate", async (req, res) => {
  const { licenseKey, productSlug } = req.body || {};
  if (!licenseKey || !productSlug) {
    return res.status(400).json({ error: "Missing licenseKey or productSlug" });
  }
  const license = await prisma.license.findUnique({
    where: { key: String(licenseKey) },
    include: { product: true },
  });
  if (
    !license ||
    license.userId !== req.user!.id ||
    license.product.slug !== productSlug ||
    license.status !== LicenseStatus.ACTIVE ||
    (license.expiresAt && license.expiresAt < new Date())
  ) {
    return res.status(403).json({ valid: false });
  }
  res.json({
    valid: true,
    license: { id: license.id, key: license.key, expiresAt: license.expiresAt },
    product: { slug: license.product.slug, version: license.product.version },
  });
});

export default router;
