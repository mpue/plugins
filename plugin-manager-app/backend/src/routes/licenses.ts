import { Router } from "express";
import { prisma } from "../db";
import { authRequired } from "../middleware/auth";

const router = Router();

router.get("/", authRequired, async (req, res) => {
  const licenses = await prisma.license.findMany({
    where: { userId: req.user!.id },
    include: {
      product: {
        include: {
          productFiles: { select: { id: true, platform: true, format: true, fileSize: true }, orderBy: { createdAt: "asc" } },
        },
      },
      activations: {
        select: { id: true, machineId: true, appVersion: true, createdAt: true },
        orderBy: { createdAt: "asc" },
      },
    },
    orderBy: { createdAt: "desc" },
  });
  res.json(licenses);
});

export default router;
