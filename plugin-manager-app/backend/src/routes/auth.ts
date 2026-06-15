import { Router } from "express";
import bcrypt from "bcryptjs";
import { z } from "zod";
import { prisma } from "../db";
import { signAccessToken } from "../services/jwt";
import { authRequired } from "../middleware/auth";
import { randomBytes } from "crypto";
import { sendWelcomeEmail, sendPasswordResetEmail } from "../services/mailer";
import { createPasswordResetToken, hashToken, resetLink } from "../services/password-reset";

const router = Router();

const registerSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
  name: z.string().min(1).optional(),
});

router.post("/register", async (req, res) => {
  const parsed = registerSchema.safeParse(req.body);
  if (!parsed.success) {
    return res.status(400).json({ error: "Invalid input", details: parsed.error.flatten() });
  }
  const { email, password, name } = parsed.data;

  const existing = await prisma.user.findUnique({ where: { email } });
  if (existing) {
    return res.status(409).json({ error: "Email already registered" });
  }

  const passwordHash = await bcrypt.hash(password, 10);
  const user = await prisma.user.create({
    data: { email, passwordHash, name },
  });

  // Welcome email — best-effort, never blocks registration.
  sendWelcomeEmail(user.email, user.name).catch((e) =>
    console.error("welcome email failed:", e)
  );

  const token = signAccessToken(user);
  res.status(201).json({
    token,
    user: { id: user.id, email: user.email, name: user.name, role: user.role },
  });
});

// --- Password reset --------------------------------------------------------
const forgotSchema = z.object({ email: z.string().email() });

router.post("/forgot-password", async (req, res) => {
  const parsed = forgotSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });

  const user = await prisma.user.findUnique({ where: { email: parsed.data.email } });
  // Always respond 200 — don't reveal whether an account exists.
  if (user) {
    try {
      const raw = await createPasswordResetToken(user.id);
      await sendPasswordResetEmail(user.email, resetLink(raw));
    } catch (e) {
      console.error("password reset email failed:", e);
    }
  }
  res.json({ ok: true });
});

const resetSchema = z.object({
  token: z.string().min(1),
  password: z.string().min(8),
});

router.post("/reset-password", async (req, res) => {
  const parsed = resetSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid input" });

  const tokenHash = hashToken(parsed.data.token);
  const record = await prisma.passwordResetToken.findUnique({ where: { tokenHash } });
  if (!record || record.usedAt || record.expiresAt < new Date()) {
    return res.status(400).json({ error: "Invalid or expired token" });
  }

  const passwordHash = await bcrypt.hash(parsed.data.password, 10);
  await prisma.$transaction([
    prisma.user.update({ where: { id: record.userId }, data: { passwordHash } }),
    prisma.passwordResetToken.update({ where: { id: record.id }, data: { usedAt: new Date() } }),
    // Invalidate any other outstanding tokens for this user.
    prisma.passwordResetToken.updateMany({
      where: { userId: record.userId, usedAt: null },
      data: { usedAt: new Date() },
    }),
  ]);

  res.json({ ok: true });
});

const loginSchema = z.object({
  email: z.string().email(),
  password: z.string().min(1),
});

router.post("/login", async (req, res) => {
  const parsed = loginSchema.safeParse(req.body);
  if (!parsed.success) {
    return res.status(400).json({ error: "Invalid input" });
  }
  const { email, password } = parsed.data;
  const user = await prisma.user.findUnique({ where: { email } });
  if (!user) return res.status(401).json({ error: "Invalid credentials" });

  const ok = await bcrypt.compare(password, user.passwordHash);
  if (!ok) return res.status(401).json({ error: "Invalid credentials" });

  const token = signAccessToken(user);
  res.json({
    token,
    user: { id: user.id, email: user.email, name: user.name, role: user.role },
  });
});

router.get("/me", authRequired, async (req, res) => {
  const user = await prisma.user.findUnique({ where: { id: req.user!.id } });
  if (!user) return res.status(404).json({ error: "Not found" });
  res.json({
    id: user.id,
    email: user.email,
    name: user.name,
    role: user.role,
    hasApiToken: Boolean(user.apiToken),
  });
});

router.post("/api-token", authRequired, async (req, res) => {
  const token = randomBytes(32).toString("hex");
  await prisma.user.update({
    where: { id: req.user!.id },
    data: { apiToken: token },
  });
  res.json({ apiToken: token });
});

router.delete("/api-token", authRequired, async (req, res) => {
  await prisma.user.update({
    where: { id: req.user!.id },
    data: { apiToken: null },
  });
  res.json({ ok: true });
});

export default router;
