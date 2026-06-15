import { Router } from "express";
import { prisma } from "../db";

const router = Router();

router.get("/", async (_req, res) => {
  const products = await prisma.product.findMany({
    where: { active: true },
    orderBy: { createdAt: "desc" },
    select: {
      id: true,
      slug: true,
      name: true,
      description: true,
      priceCents: true,
      currency: true,
      imageUrl: true,
      version: true,
      createdAt: true,
      productFiles: { select: { id: true, platform: true, format: true, fileSize: true }, orderBy: { createdAt: "asc" } },
    },
  });
  res.json(products);
});

router.get("/:slug", async (req, res) => {
  const product = await prisma.product.findUnique({
    where: { slug: req.params.slug },
    select: {
      id: true,
      slug: true,
      name: true,
      description: true,
      priceCents: true,
      currency: true,
      imageUrl: true,
      version: true,
      active: true,
      productFiles: { select: { id: true, platform: true, format: true, fileSize: true }, orderBy: { createdAt: "asc" } },
    },
  });
  if (!product || !product.active) return res.status(404).json({ error: "Not found" });
  res.json(product);
});

export default router;
