import { Router } from "express";
import { prisma } from "../db";

const router = Router();

router.get("/public", async (_req, res) => {
  const items = await prisma.systemConfig.findMany({
    where: {
      key: { in: ["site.title", "site.tagline", "site.homeHtml"] },
    },
  });
  const map: Record<string, string> = {};
  for (const i of items) map[i.key] = i.value;
  res.json(map);
});

export default router;
