const API_BASE = process.env.API_BASE_URL || "";

let token: string | null = localStorage.getItem("token");

export function setToken(t: string | null) {
  token = t;
  if (t) localStorage.setItem("token", t);
  else localStorage.removeItem("token");
}

export function getToken() {
  return token;
}

async function parseResponseBody(res: Response): Promise<any> {
  const text = await res.text();
  if (!text) return null;

  const contentType = res.headers.get("content-type") || "";
  if (contentType.includes("application/json")) {
    try {
      return JSON.parse(text);
    } catch {
      return { error: "Invalid JSON response from server" };
    }
  }

  return { error: text.replace(/<[^>]*>/g, " ").replace(/\s+/g, " ").trim() };
}

export async function api<T = any>(
  path: string,
  options: RequestInit = {}
): Promise<T> {
  const headers: Record<string, string> = {
    "Content-Type": "application/json",
    ...((options.headers as Record<string, string>) || {}),
  };
  if (token) headers["Authorization"] = `Bearer ${token}`;

  const res = await fetch(`${API_BASE}/api${path}`, { ...options, headers });
  const data = await parseResponseBody(res);
  if (!res.ok) {
    const msg = (data && data.error) || `HTTP ${res.status}`;
    throw new Error(msg);
  }
  return data as T;
}

export async function apiUpload<T = any>(
  path: string,
  file: File,
  fieldName = "file"
): Promise<T> {
  const fd = new FormData();
  fd.append(fieldName, file);
  const headers: Record<string, string> = {};
  if (token) headers["Authorization"] = `Bearer ${token}`;
  const res = await fetch(`${API_BASE}/api${path}`, {
    method: "POST",
    body: fd,
    headers,
  });
  const data = await parseResponseBody(res);
  if (!res.ok) {
    const msg = (data && data.error) || `HTTP ${res.status}`;
    throw new Error(msg);
  }
  return data as T;
}

export function assetUrl(p: string | null | undefined): string {
  if (!p) return "";
  if (/^https?:\/\//i.test(p)) return p;
  return `${API_BASE}${p}`;
}

export function downloadUrl(token: string): string {
  return `${API_BASE}/api/downloads/file?token=${encodeURIComponent(token)}`;
}
