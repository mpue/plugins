import { PrismaClient, Role } from "@prisma/client";
import bcrypt from "bcryptjs";

/*
 * Resets (or creates) the admin login. Unlike the seed, this overwrites the
 * password even if the user already exists — use it when the admin credentials
 * were lost.
 *
 * Usage:
 *   npx ts-node scripts/reset-admin.ts                       # uses ADMIN_EMAIL / ADMIN_PASSWORD env, else defaults
 *   npx ts-node scripts/reset-admin.ts you@example.com newpass123
 *
 * Needs DATABASE_URL (loaded from .env). Make sure the DB is up.
 */

const prisma = new PrismaClient();

async function main() {
  const email = process.argv[2] || process.env.ADMIN_EMAIL || "admin@example.com";
  const password = process.argv[3] || process.env.ADMIN_PASSWORD || "admin12345";

  const passwordHash = await bcrypt.hash(password, 10);

  const user = await prisma.user.upsert({
    where: { email },
    update: { passwordHash, role: Role.ADMIN },
    create: { email, passwordHash, name: "Admin", role: Role.ADMIN },
  });

  console.log(`Admin ready: ${user.email} (role ${user.role})`);
  console.log(`Password set to: ${password}`);
}

main()
  .catch((e) => {
    console.error(e);
    process.exit(1);
  })
  .finally(async () => {
    await prisma.$disconnect();
  });
