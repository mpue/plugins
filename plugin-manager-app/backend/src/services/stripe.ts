import Stripe from "stripe";
import { config } from "../config";

export const stripe = config.stripe.secretKey
  ? new Stripe(config.stripe.secretKey, { apiVersion: "2024-10-28.acacia" as any })
  : (null as unknown as Stripe);

export function ensureStripe(): Stripe {
  if (!stripe) {
    throw Object.assign(new Error("Stripe is not configured"), { status: 500 });
  }
  return stripe;
}
