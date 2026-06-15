import { NextFunction, Request, Response } from "express";
import jwt from "jsonwebtoken";
import { config } from "../config";
import { prisma } from "../db";
import { Role } from "@prisma/client";

interface JwtPayload {
  sub: string;
  email: string;
  role: Role;
}

export function authRequired(req: Request, res: Response, next: NextFunction) {
  const header = req.headers.authorization;
  if (!header || !header.startsWith("Bearer ")) {
    return res.status(401).json({ error: "Unauthorized" });
  }
  const token = header.slice("Bearer ".length);
  try {
    const payload = jwt.verify(token, config.jwtSecret) as JwtPayload;
    req.user = { id: payload.sub, email: payload.email, role: payload.role };
    next();
  } catch {
    return res.status(401).json({ error: "Invalid token" });
  }
}

export function adminRequired(req: Request, res: Response, next: NextFunction) {
  if (!req.user || req.user.role !== Role.ADMIN) {
    return res.status(403).json({ error: "Forbidden" });
  }
  next();
}

export async function apiTokenAuth(req: Request, res: Response, next: NextFunction) {
  const header = req.headers.authorization;
  let token: string | null = null;
  if (header && header.startsWith("Bearer ")) {
    token = header.slice("Bearer ".length);
  } else if (typeof req.headers["x-api-token"] === "string") {
    token = req.headers["x-api-token"] as string;
  }
  if (!token) return res.status(401).json({ error: "Missing API token" });

  const user = await prisma.user.findUnique({ where: { apiToken: token } });
  if (!user) return res.status(401).json({ error: "Invalid API token" });

  req.user = { id: user.id, email: user.email, role: user.role };
  next();
}
