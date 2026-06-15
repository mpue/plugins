import { createPrivateKey, sign, KeyObject } from "crypto";
import { config } from "../config";

// Token payload signed by the server and verified offline by the Synthlab client
// against the embedded Ed25519 public key. No `exp` — perpetual license.
export interface ActivationTokenPayload {
  v: number;
  product: string;
  lic: string; // license id
  mid: string; // machine hash (sha256 hex) as supplied by the client
  iat: number; // unix seconds
}

function base64url(buf: Buffer): string {
  return buf
    .toString("base64")
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=+$/, "");
}

let cachedKey: KeyObject | null = null;

function getPrivateKey(): KeyObject {
  if (cachedKey) return cachedKey;
  if (!config.licenseSigningPrivateKey) {
    throw new Error("LICENSE_SIGNING_PRIVATE_KEY is not configured");
  }
  cachedKey = createPrivateKey({
    key: config.licenseSigningPrivateKey,
    format: "pem",
    type: "pkcs8",
  });
  return cachedKey;
}

/**
 * Produces `<base64url payload>.<base64url ed25519-sig>`.
 *
 * The signature is computed over the EXACT JSON payload bytes — the same bytes
 * the client obtains when it base64url-decodes the payload segment. The client
 * verifies over those decoded bytes (never over re-serialized JSON), so the two
 * sides must agree byte-for-byte. Do not pretty-print or re-order after signing.
 */
export function signActivationToken(payload: ActivationTokenPayload): string {
  const payloadBytes = Buffer.from(JSON.stringify(payload), "utf8");
  // Ed25519: algorithm MUST be null; returns a raw 64-byte detached signature,
  // compatible with libsodium crypto_sign_verify_detached.
  const signature = sign(null, payloadBytes, getPrivateKey());
  return `${base64url(payloadBytes)}.${base64url(signature)}`;
}
