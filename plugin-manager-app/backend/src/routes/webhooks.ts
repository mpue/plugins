import { Router, raw } from "express";
import { prisma } from "../db";
import { ensureStripe } from "../services/stripe";
import { config } from "../config";
import { generateLicenseKey } from "../services/license";
import { sendLicenseKeyEmail } from "../services/mailer";
import { OrderStatus } from "@prisma/client";

const router = Router();

router.post("/stripe", raw({ type: "application/json" }), async (req, res) => {
  const stripe = ensureStripe();
  const sig = req.headers["stripe-signature"];
  if (!sig || !config.stripe.webhookSecret) {
    return res.status(400).send("Missing signature or webhook secret");
  }

  let event;
  try {
    event = stripe.webhooks.constructEvent(req.body, sig as string, config.stripe.webhookSecret);
  } catch (err: any) {
    console.error("Webhook signature verification failed:", err.message);
    return res.status(400).send(`Webhook Error: ${err.message}`);
  }

  try {
    if (event.type === "checkout.session.completed") {
      const session = event.data.object as any;
      const orderId: string | undefined = session.metadata?.orderId;
      if (!orderId) return res.json({ received: true });

      const order = await prisma.order.findUnique({
        where: { id: orderId },
        include: { items: true },
      });
      if (!order) return res.json({ received: true });
      if (order.status === OrderStatus.PAID) return res.json({ received: true });

      await prisma.$transaction(async (tx) => {
        await tx.order.update({
          where: { id: order.id },
          data: {
            status: OrderStatus.PAID,
            stripePaymentIntentId:
              typeof session.payment_intent === "string" ? session.payment_intent : null,
          },
        });

        for (const item of order.items) {
          for (let i = 0; i < item.quantity; i++) {
            await tx.license.create({
              data: {
                key: generateLicenseKey(),
                userId: order.userId,
                productId: item.productId,
                orderId: order.id,
              },
            });
          }
        }
      });

      // Email the freshly-issued license keys to the buyer — best-effort.
      const issued = await prisma.license.findMany({
        where: { orderId: order.id },
        include: { user: true, product: true },
      });
      for (const lic of issued) {
        sendLicenseKeyEmail(lic.user.email, lic.product.name, lic.key).catch((e) =>
          console.error("purchase license email failed:", e)
        );
      }
    } else if (
      event.type === "checkout.session.expired" ||
      event.type === "checkout.session.async_payment_failed"
    ) {
      const session = event.data.object as any;
      const orderId: string | undefined = session.metadata?.orderId;
      if (orderId) {
        await prisma.order.update({
          where: { id: orderId },
          data: { status: OrderStatus.FAILED },
        });
      }
    }
  } catch (err) {
    console.error("Webhook handler error:", err);
    return res.status(500).send("Webhook handler error");
  }

  res.json({ received: true });
});

export default router;
