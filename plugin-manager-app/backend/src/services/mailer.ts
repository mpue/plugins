import nodemailer, { Transporter } from "nodemailer";
import { config } from "../config";

let transporter: Transporter | null = null;

export function isMailEnabled(): boolean {
  return Boolean(config.mail.host);
}

function getTransport(): Transporter | null {
  if (!isMailEnabled()) return null;
  if (!transporter) {
    transporter = nodemailer.createTransport({
      host: config.mail.host,
      port: config.mail.port,
      secure: config.mail.secure,
      auth: config.mail.user ? { user: config.mail.user, pass: config.mail.pass } : undefined,
    });
  }
  return transporter;
}

/** Low-level send. Returns true if dispatched, false if mail is disabled. Never throws. */
export async function sendMail(opts: {
  to: string;
  subject: string;
  html: string;
  text?: string;
}): Promise<boolean> {
  const t = getTransport();
  if (!t) {
    console.warn(`[mail] SMTP not configured — skipping "${opts.subject}" -> ${opts.to}`);
    return false;
  }
  try {
    await t.sendMail({
      from: config.mail.from,
      to: opts.to,
      subject: opts.subject,
      html: opts.html,
      text: opts.text ?? stripHtml(opts.html),
    });
    return true;
  } catch (err) {
    console.error(`[mail] failed to send "${opts.subject}" -> ${opts.to}:`, err);
    return false;
  }
}

function stripHtml(html: string): string {
  return html.replace(/<[^>]+>/g, "").replace(/\n\s*\n\s*\n/g, "\n\n").trim();
}

// --- minimal branded HTML layout -------------------------------------------
function layout(title: string, bodyHtml: string): string {
  return `<!DOCTYPE html><html><body style="margin:0;background:#eef6fa;font-family:Arial,Helvetica,sans-serif;color:#0e2a35">
  <div style="max-width:560px;margin:0 auto;padding:24px">
    <div style="font-weight:bold;letter-spacing:.12em;text-transform:uppercase;color:#0097c2;font-size:14px;margin-bottom:18px">Pueski Audio</div>
    <div style="background:#ffffff;border:1px solid rgba(0,150,180,.22);border-radius:10px;padding:28px">
      <h1 style="margin:0 0 16px;font-size:20px;color:#0e2a35">${title}</h1>
      ${bodyHtml}
    </div>
    <div style="color:#5a7d8a;font-size:12px;margin-top:18px">Pueski Audio Professional · This is an automated message.</div>
  </div></body></html>`;
}

function button(href: string, label: string): string {
  return `<a href="${href}" style="display:inline-block;background:#0097c2;color:#ffffff;text-decoration:none;padding:11px 20px;border-radius:6px;font-size:14px;letter-spacing:.04em">${label}</a>`;
}

const esc = (s: string) =>
  String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

// --- specific emails --------------------------------------------------------
export async function sendWelcomeEmail(to: string, name?: string | null) {
  const greeting = name ? `Hi ${esc(name)},` : "Hi,";
  return sendMail({
    to,
    subject: "Welcome to Pueski Audio",
    html: layout(
      "Welcome aboard",
      `<p>${greeting}</p>
       <p>Your account <strong>${esc(to)}</strong> is ready. You can sign in any time to manage your licenses and downloads.</p>
       <p style="margin:22px 0">${button(`${config.appBaseUrl}/login`, "Sign in")}</p>`
    ),
  });
}

export async function sendPasswordResetEmail(to: string, link: string, isNewAccount = false) {
  const title = isNewAccount ? "Set your password" : "Reset your password";
  const intro = isNewAccount
    ? "An account was created for you. Set a password to get started:"
    : "We received a request to reset your password. Click below to choose a new one:";
  return sendMail({
    to,
    subject: title,
    html: layout(
      title,
      `<p>${intro}</p>
       <p style="margin:22px 0">${button(link, title)}</p>
       <p style="color:#5a7d8a;font-size:13px">This link expires in ${config.passwordResetTtlMinutes} minutes. If you didn't request this, you can ignore this email.</p>`
    ),
  });
}

export async function sendLicenseKeyEmail(to: string, productName: string, key: string) {
  return sendMail({
    to,
    subject: `Your ${productName} license key`,
    html: layout(
      "Your license key",
      `<p>Thanks for choosing <strong>${esc(productName)}</strong>. Here is your license key:</p>
       <p style="margin:18px 0"><span style="display:inline-block;background:#e3f0f5;border:1px solid rgba(0,150,180,.22);border-radius:6px;padding:10px 16px;font-family:'Courier New',monospace;font-size:16px;letter-spacing:.06em;color:#0e2a35">${esc(key)}</span></p>
       <p>Enter your email and this key in the app to activate. You can view your licenses and downloads any time:</p>
       <p style="margin:22px 0">${button(`${config.appBaseUrl}/profile`, "My licenses")}</p>`
    ),
  });
}
