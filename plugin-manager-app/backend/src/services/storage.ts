import fs from "fs";
import path from "path";
import multer from "multer";
import { v4 as uuid } from "uuid";
import { config } from "../config";

function ensureDir(dir: string) {
  fs.mkdirSync(dir, { recursive: true });
}

ensureDir(config.uploadDir);
ensureDir(config.productFileDir);

const imageStorage = multer.diskStorage({
  destination: (_req, _file, cb) => {
    ensureDir(config.uploadDir);
    cb(null, config.uploadDir);
  },
  filename: (_req, file, cb) => {
    const ext = path.extname(file.originalname).toLowerCase();
    cb(null, `${uuid()}${ext}`);
  },
});

const productFileStorage = multer.diskStorage({
  destination: (_req, _file, cb) => {
    ensureDir(config.productFileDir);
    cb(null, config.productFileDir);
  },
  filename: (_req, file, cb) => {
    const ext = path.extname(file.originalname);
    cb(null, `${uuid()}${ext}`);
  },
});

export const uploadImage = multer({
  storage: imageStorage,
  limits: { fileSize: 10 * 1024 * 1024 },
  fileFilter: (_req, file, cb) => {
    if (!/^image\//.test(file.mimetype)) {
      return cb(new Error("Only image uploads allowed"));
    }
    cb(null, true);
  },
});

export const uploadProductFile = multer({
  storage: productFileStorage,
  limits: { fileSize: 2 * 1024 * 1024 * 1024 },
});

export function imagePublicUrl(filename: string): string {
  return `/uploads/${filename}`;
}

export function productFilePath(filename: string): string {
  return path.join(config.productFileDir, filename);
}

export function deleteFileSafe(absPath: string) {
  try {
    if (fs.existsSync(absPath)) fs.unlinkSync(absPath);
  } catch (e) {
    console.warn("Failed to delete file:", absPath, e);
  }
}
