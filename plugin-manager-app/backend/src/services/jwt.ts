import jwt, { SignOptions } from "jsonwebtoken";
import { config } from "../config";
import { Role } from "@prisma/client";

export function signAccessToken(user: { id: string; email: string; role: Role }) {
  const opts: SignOptions = { expiresIn: config.jwtExpiresIn as any };
  return jwt.sign(
    { sub: user.id, email: user.email, role: user.role },
    config.jwtSecret,
    opts
  );
}

export function signDownloadToken(payload: { userId: string; productId: string; licenseId: string; productFileId?: string }) {
  return jwt.sign(payload, config.jwtSecret, {
    expiresIn: config.downloadTokenTtlSeconds,
  });
}

export function verifyDownloadToken(token: string): {
  userId: string;
  productId: string;
  licenseId: string;
  productFileId?: string;
} {
  return jwt.verify(token, config.jwtSecret) as any;
}
