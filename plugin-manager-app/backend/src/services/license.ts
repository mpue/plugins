import { randomBytes } from "crypto";

export function generateLicenseKey(): string {
  const segments = Array.from({ length: 4 }, () =>
    randomBytes(2).toString("hex").toUpperCase()
  );
  return segments.join("-");
}
