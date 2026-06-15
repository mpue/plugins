import { Router } from "express";
import fs from "fs";
import path from "path";
import { prisma } from "../db";
import { authRequired } from "../middleware/auth";
import { signDownloadToken, verifyDownloadToken } from "../services/jwt";
import { productFilePath } from "../services/storage";
import { LicenseStatus } from "@prisma/client";

const router = Router();

// POST /token/:licenseId?fileId=<productFileId>
router.post("/token/:licenseId", authRequired, async (req, res) => {
  const license = await prisma.license.findFirst({
    where: { id: req.params.licenseId, userId: req.user!.id },
    include: { product: { include: { productFiles: true } } },
  });
  if (!license) return res.status(404).json({ error: "License not found" });
  if (license.status !== LicenseStatus.ACTIVE) {
    return res.status(403).json({ error: "License is not active" });
  }

  const fileId = (req.query.fileId as string) || "";
  if (fileId) {
    const found = license.product.productFiles.find((f) => f.id === fileId);
    if (!found) return res.status(404).json({ error: "Product file not found" });
  } else if (license.product.productFiles.length === 0) {
    return res.status(404).json({ error: "Product has no downloadable files" });
  }

  const token = signDownloadToken({
    userId: req.user!.id,
    productId: license.productId,
    licenseId: license.id,
    productFileId: fileId || undefined,
  });
  res.json({ token });
});

router.get("/file", async (req, res) => {
  const token = (req.query.token as string) || "";
  if (!token) return res.status(400).json({ error: "Missing token" });

  let payload;
  try {
    payload = verifyDownloadToken(token);
  } catch {
    return res.status(401).json({ error: "Invalid or expired token" });
  }

  const license = await prisma.license.findUnique({
    where: { id: payload.licenseId },
    include: { product: { include: { productFiles: true } } },
  });
  if (!license || license.userId !== payload.userId) {
    return res.status(404).json({ error: "Not found" });
  }
  if (license.status !== LicenseStatus.ACTIVE) {
    return res.status(403).json({ error: "License is not active" });
  }

  const pf = payload.productFileId
    ? license.product.productFiles.find((f) => f.id === payload.productFileId)
    : license.product.productFiles[0];

  if (!pf) return res.status(404).json({ error: "Product has no file" });

  const abs = productFilePath(pf.fileName);
  if (!fs.existsSync(abs)) return res.status(404).json({ error: "File missing on server" });

  const safeName = `${license.product.slug}-${license.product.version}-${pf.platform.toLowerCase()}-${pf.format.toLowerCase()}${path.extname(abs)}`;
  res.setHeader("Content-Disposition", `attachment; filename="${safeName}"`);
  res.setHeader("Content-Type", "application/octet-stream");
  fs.createReadStream(abs).pipe(res);
});

export default router;
