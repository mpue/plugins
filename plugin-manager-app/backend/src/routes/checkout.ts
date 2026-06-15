import { NextFunction, Router } from "express";
import { z } from "zod";
import { prisma } from "../db";
import { authRequired } from "../middleware/auth";
import { ensureStripe } from "../services/stripe";
import { config } from "../config";

const router = Router();

const checkoutSchema = z.object({
  items: z
    .array(
      z.object({
        productId: z.string().uuid(),
        quantity: z.number().int().min(1).max(10).default(1),
      })
    )
    .min(1),
});

router.post("/session", authRequired, async (req, res, next: NextFunction) => {
  try {
    const parsed = checkoutSchema.safeParse(req.body);
    if (!parsed.success) {
      return res.status(400).json({ error: "Invalid input", details: parsed.error.flatten() });
    }
    const stripe = ensureStripe();

    const productIds = parsed.data.items.map((i) => i.productId);
    const products = await prisma.product.findMany({
      where: { id: { in: productIds }, active: true },
    });
    if (products.length !== productIds.length) {
      return res.status(400).json({ error: "One or more products are unavailable" });
    }

    const totalCents = parsed.data.items.reduce((sum, item) => {
      const p = products.find((pr) => pr.id === item.productId)!;
      return sum + p.priceCents * item.quantity;
    }, 0);

    const order = await prisma.order.create({
      data: {
        userId: req.user!.id,
        totalCents,
        currency: products[0].currency,
        items: {
          create: parsed.data.items.map((item) => {
            const p = products.find((pr) => pr.id === item.productId)!;
            return {
              productId: p.id,
              priceCents: p.priceCents,
              quantity: item.quantity,
            };
          }),
        },
      },
    });

    const session = await stripe.checkout.sessions.create({
      mode: "payment",
      customer_email: req.user!.email,
      success_url: `${config.frontendUrl}/checkout/success?session_id={CHECKOUT_SESSION_ID}`,
      cancel_url: `${config.frontendUrl}/checkout/cancel`,
      line_items: parsed.data.items.map((item) => {
        const p = products.find((pr) => pr.id === item.productId)!;
        return {
          quantity: item.quantity,
          price_data: {
            currency: p.currency.toLowerCase(),
            unit_amount: p.priceCents,
            product_data: {
              name: p.name,
              description: p.description.slice(0, 500),
              images: p.imageUrl ? [`${config.frontendUrl}${p.imageUrl}`] : undefined,
            },
          },
        };
      }),
      metadata: {
        orderId: order.id,
        userId: req.user!.id,
      },
    });

    await prisma.order.update({
      where: { id: order.id },
      data: { stripeSessionId: session.id },
    });

    res.json({ id: session.id, url: session.url });
  } catch (err) {
    next(err);
  }
});

// Called by the frontend after Stripe redirects back to the success URL.
// Fetches the session directly from Stripe and finalises the order (creates
// licenses) if payment_status is "paid". This is the fallback for cases where
// the Stripe webhook does not reach the server (e.g. local dev).
router.post("/verify", authRequired, async (req, res, next: NextFunction) => {
  try {
    const sessionId =
      (req.query.session_id as string | undefined) ||
      (req.body?.session_id as string | undefined);
    if (!sessionId) {
      return res.status(400).json({ error: "session_id is required" });
    }

    const stripe = ensureStripe();
    const session = await stripe.checkout.sessions.retrieve(sessionId, {
      expand: ["line_items"],
    });

    if (session.payment_status !== "paid") {
      return res.status(402).json({ error: "Payment not completed" });
    }

    const orderId: string | undefined = (session.metadata as any)?.orderId;
    if (!orderId) {
      return res.status(400).json({ error: "No orderId in session metadata" });
    }

    const order = await prisma.order.findFirst({
      where: { id: orderId, userId: req.user!.id },
      include: { items: true },
    });
    if (!order) {
      return res.status(404).json({ error: "Order not found" });
    }

    // Idempotent: if already paid just return the licenses.
    if (order.status !== "PENDING") {
      const licenses = await prisma.license.findMany({
        where: { orderId: order.id },
        include: { product: true },
      });
      return res.json({ alreadyProcessed: true, licenses });
    }

    const { generateLicenseKey } = await import("../services/license");

    const licenses = await prisma.$transaction(async (tx) => {
      await tx.order.update({
        where: { id: order.id },
        data: {
          status: "PAID",
          stripePaymentIntentId:
            typeof session.payment_intent === "string" ? session.payment_intent : null,
        },
      });

      const created: Awaited<ReturnType<typeof tx.license.create>>[] = [];
      for (const item of order.items) {
        for (let i = 0; i < item.quantity; i++) {
          const license = await tx.license.create({
            data: {
              key: generateLicenseKey(),
              userId: order.userId,
              productId: item.productId,
              orderId: order.id,
            },
            include: { product: true },
          });
          created.push(license);
        }
      }
      return created;
    });

    res.json({ licenses });
  } catch (err) {
    next(err);
  }
});

export default router;
