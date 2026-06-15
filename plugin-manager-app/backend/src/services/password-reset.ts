import { randomBytes, createHash } from "crypto";
import { prisma } from "../db";
import { config } from "../config";

export function hashToken(raw: string): string {
  return createHash("sha256").update(raw).digest("hex");
}

/** Creates a password-reset token for a user and returns the RAW token to email. */
export async function createPasswordResetToken(userId: string): Promise<string> {
  const raw = randomBytes(32).toString("hex");
  const tokenHash = hashToken(raw);
  const expiresAt = new Date(Date.now() + config.passwordResetTtlMinutes * 60 * 1000);
  await prisma.passwordResetToken.create({ data: { userId, tokenHash, expiresAt } });
  return raw;
}

export function resetLink(rawToken: string): string {
  return `${config.appBaseUrl}/reset-password?token=${encodeURIComponent(rawToken)}`;
}
