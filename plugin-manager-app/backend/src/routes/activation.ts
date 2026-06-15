import { Router } from "express";
import { z } from "zod";
import { LicenseStatus } from "@prisma/client";
import { prisma } from "../db";
import { signActivationToken } from "../services/activation-token";

// Public license-activation endpoints consumed by the Synthlab desktop client.
//
// These are intentionally UNAUTHENTICATED (no JWT): the desktop app only holds
// the license key + email, which together are the credential. Every response is
// one of the contract-defined error codes — never a leaking internal message:
//   invalid_key | refunded | activation_limit_reached | unknown
//
// Seat-counting (max activations per license, soft machine binding) is done here.
const router = Router();

const activateSchema = z.object({
  product: z.string().min(1),
  license_key: z.string().min(1),
  email: z.string().email(),
  machine_id: z.string().min(1),
  app_version: z.string().optional(),
});

const deactivateSchema = z.object({
  product: z.string().min(1),
  license_key: z.string().min(1),
  machine_id: z.string().min(1),
});

// POST /activate
// -> 200 { token } | 4xx { error }
router.post("/activate", async (req, res) => {
  const parsed = activateSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "unknown" });

  const { product, license_key, email, machine_id, app_version } = parsed.data;

  try {
    const license = await prisma.license.findUnique({
      where: { key: license_key },
      include: { user: true, product: true },
    });

    // Key unknown, wrong product, or email doesn't match the owner -> invalid_key.
    if (!license) return res.status(404).json({ error: "invalid_key" });
    if (license.product.slug !== product)
      return res.status(404).json({ error: "invalid_key" });
    if (license.user.email.toLowerCase() !== email.trim().toLowerCase())
      return res.status(404).json({ error: "invalid_key" });

    if (license.status === LicenseStatus.REVOKED)
      return res.status(403).json({ error: "refunded" });
    if (license.status !== LicenseStatus.ACTIVE)
      return res.status(403).json({ error: "invalid_key" });

    // Seat counting in a transaction so concurrent activations can't both slip
    // past the limit. One row per (license, machine); same machine reuses its
    // seat. The token is signed INSIDE the transaction: if signing throws, the
    // seat row is rolled back, so a failed activation never consumes a seat.
    const result = await prisma.$transaction(async (tx) => {
      const existing = await tx.activation.findUnique({
        where: { licenseId_machineId: { licenseId: license.id, machineId: machine_id } },
      });

      if (existing) {
        await tx.activation.update({
          where: { id: existing.id },
          data: { appVersion: app_version ?? null },
        });
      } else {
        const used = await tx.activation.count({ where: { licenseId: license.id } });
        if (used >= license.maxActivations) return { limit: true as const };

        await tx.activation.create({
          data: { licenseId: license.id, machineId: machine_id, appVersion: app_version ?? null },
        });
      }

      const token = signActivationToken({
        v: 1,
        product,
        lic: license.id,
        mid: machine_id,
        iat: Math.floor(Date.now() / 1000),
      });
      return { token };
    });

    if ("limit" in result)
      return res.status(409).json({ error: "activation_limit_reached" });

    return res.json({ token: result.token });
  } catch (err) {
    console.error("activation/activate failed:", err);
    return res.status(500).json({ error: "unknown" });
  }
});

// POST /deactivate
// -> 200 { ok: true } | 4xx { error }
// Frees the seat for this machine so it can be activated elsewhere. Idempotent.
router.post("/deactivate", async (req, res) => {
  const parsed = deactivateSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "unknown" });

  const { product, license_key, machine_id } = parsed.data;

  try {
    const license = await prisma.license.findUnique({
      where: { key: license_key },
      include: { product: true },
    });

    if (!license || license.product.slug !== product)
      return res.status(404).json({ error: "invalid_key" });

    await prisma.activation.deleteMany({
      where: { licenseId: license.id, machineId: machine_id },
    });

    return res.json({ ok: true });
  } catch (err) {
    console.error("activation/deactivate failed:", err);
    return res.status(500).json({ error: "unknown" });
  }
});

export default router;
