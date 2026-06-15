import { createApp } from "./server";
import { config } from "./config";
import { prisma } from "./db";
import bcrypt from "bcryptjs";
import { Role } from "@prisma/client";

async function bootstrap() {
  // Auto-seed admin if missing.
  try {
    const existing = await prisma.user.findUnique({ where: { email: config.adminEmail } });
    if (!existing) {
      const passwordHash = await bcrypt.hash(config.adminPassword, 10);
      await prisma.user.create({
        data: {
          email: config.adminEmail,
          passwordHash,
          name: "Admin",
          role: Role.ADMIN,
        },
      });
      console.log(`Bootstrapped admin user ${config.adminEmail}`);
    }
  } catch (e) {
    console.warn("Admin bootstrap skipped:", (e as Error).message);
  }

  const app = createApp();
  app.listen(config.port, () => {
    console.log(`API listening on :${config.port}`);
  });
}

bootstrap().catch((e) => {
  console.error(e);
  process.exit(1);
});
