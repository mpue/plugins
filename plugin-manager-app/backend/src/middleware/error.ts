import { NextFunction, Request, Response } from "express";

export function notFound(_req: Request, res: Response) {
  res.status(404).json({ error: "Not Found" });
}

export function errorHandler(
  err: unknown,
  _req: Request,
  res: Response,
  _next: NextFunction
) {
  console.error(err);
  const status = (err as any)?.status || 500;
  const message = (err as any)?.message || "Internal Server Error";
  res.status(status).json({ error: message });
}
