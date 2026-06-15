#!/usr/bin/env node
/*
 * Generates an Ed25519 keypair for the license-activation system.
 *
 *   - The PRIVATE key (PKCS8 PEM) goes into the backend env as
 *     LICENSE_SIGNING_PRIVATE_KEY (single line, newlines escaped as \n).
 *     KEEP IT SECRET — it signs every activation token.
 *
 *   - The PUBLIC key is printed twice:
 *       * raw 32-byte key, standard base64  -> paste into the Synthlab client
 *         (LicenseManager.cpp, kPublicKeyB64). libsodium expects exactly these
 *         32 bytes for crypto_sign_verify_detached.
 *       * SPKI PEM (informational).
 *
 * Usage:  node scripts/generate-license-keypair.js
 */

const { generateKeyPairSync } = require("crypto");

const { publicKey, privateKey } = generateKeyPairSync("ed25519");

const privPem = privateKey.export({ type: "pkcs8", format: "pem" });
const pubPem = publicKey.export({ type: "spki", format: "pem" });

// Raw 32-byte public key (Edwards point) via JWK, then standard base64 for JUCE.
const jwk = publicKey.export({ format: "jwk" });
const pubRaw = Buffer.from(jwk.x, "base64url");
const pubB64 = pubRaw.toString("base64");

const privEnv = privPem.trim().replace(/\n/g, "\\n");

console.log("=== Ed25519 license signing keypair ===\n");
console.log("Backend env (.env / docker-compose), single line:\n");
console.log(`LICENSE_SIGNING_PRIVATE_KEY="${privEnv}"\n`);
console.log("Synthlab client (Source/Licensing/LicenseManager.cpp, kPublicKeyB64):\n");
console.log(`  "${pubB64}"   // ${pubRaw.length} bytes\n`);
console.log("Public key (SPKI PEM, informational):\n");
console.log(pubPem);
