import { Router } from "express";
import { prisma } from "../db";
import { authRequired } from "../middleware/auth";

const router = Router();

router.get("/", authRequired, async (req, res) => {
  const orders = await prisma.order.findMany({
    where: { userId: req.user!.id },
    include: {
      items: { include: { product: true } },
      licenses: { include: { product: true } },
    },
    orderBy: { createdAt: "desc" },
  });
  res.json(orders);
});

router.get("/:id", authRequired, async (req, res) => {
  const order = await prisma.order.findFirst({
    where: { id: req.params.id, userId: req.user!.id },
    include: {
      items: { include: { product: true } },
      licenses: { include: { product: true } },
    },
  });
  if (!order) return res.status(404).json({ error: "Not found" });
  res.json(order);
});

export default router;
