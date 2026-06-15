import { PrismaClient, Role } from "@prisma/client";
import bcrypt from "bcryptjs";

const prisma = new PrismaClient();

async function main() {
  const adminEmail = process.env.ADMIN_EMAIL || "admin@example.com";
  const adminPassword = process.env.ADMIN_PASSWORD || "admin12345";

  const existing = await prisma.user.findUnique({ where: { email: adminEmail } });
  if (!existing) {
    const passwordHash = await bcrypt.hash(adminPassword, 10);
    await prisma.user.create({
      data: { email: adminEmail, passwordHash, name: "Admin", role: Role.ADMIN },
    });
    console.log(`Created admin user ${adminEmail}`);
  } else {
    console.log(`Admin user ${adminEmail} already exists`);
  }

  const defaults: Array<[string, string]> = [
    ["site.title", "Plugin Manager"],
    ["site.tagline", "Professional audio & software plugins"],
    ["site.homeHtml", "<h1>Welcome</h1><p>Discover our plugins in the shop.</p>"],
  ];
  for (const [key, value] of defaults) {
    await prisma.systemConfig.upsert({
      where: { key },
      update: {},
      create: { key, value },
    });
  }
}

main()
  .catch((e) => {
    console.error(e);
    process.exit(1);
  })
  .finally(async () => {
    await prisma.$disconnect();
  });
