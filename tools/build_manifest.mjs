#!/usr/bin/env node

import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";

const root = process.cwd();
const output = path.join(root, "build", "reports", "build-manifest.json");
const roots = [
  "Makefile", "config", "include", "kernel", "compat", "posix", "arch",
  "tests", "tools",
];

function filesUnder(entry) {
  const absolute = path.join(root, entry);
  const stat = fs.statSync(absolute);
  if (stat.isFile()) return [entry];
  return fs.readdirSync(absolute, { withFileTypes: true }).flatMap((item) => {
    const relative = path.join(entry, item.name);
    return item.isDirectory() ? filesUnder(relative) : [relative];
  });
}

function sha256(file) {
  return crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");
}

function version(command) {
  try {
    return execFileSync(command, ["--version"], { encoding: "utf8" })
      .split("\n")[0];
  } catch {
    return "unavailable";
  }
}

function macroValue(name) {
  const header = fs.readFileSync(
    path.join(root, "include", "aixos", "version.h"), "utf8");
  const match = header.match(new RegExp(
    `^#define\\s+${name}\\s+(0x[0-9A-Fa-f]+)u?\\s*$`, "m"));
  if (!match) throw new Error(`missing ${name} in include/aixos/version.h`);
  return match[1];
}

const files = roots.flatMap(filesUnder).sort().map((relative) => ({
  path: relative,
  sha256: sha256(path.join(root, relative)),
})).filter((entry) => !entry.path.endsWith(".lock"));
const manifest = {
  schema: 1,
  generated_utc: new Date().toISOString(),
  api_version: macroValue("AIXOS_API_VERSION"),
  abi_version: macroValue("AIXOS_ABI_VERSION"),
  compilers: {
    host: version(process.env.HOST_CC || "cc"),
    arm: version(process.env.ARM_CC || "arm-none-eabi-gcc"),
    riscv: version(process.env.RISCV_CC || "riscv-none-elf-gcc"),
  },
  artifacts: ["build/arm/cortex-m3/AIXOS.elf",
              "build/arm/cortex-m3/AIXOS.map",
              "build/arm/cortex-a55/AIXOS.elf",
              "build/arm/cortex-a55/AIXOS.map",
              "build/riscv/AIXOS.elf", "build/riscv/AIXOS.map"]
    .filter((file) => fs.existsSync(path.join(root, file)))
    .map((file) => ({ path: file, sha256: sha256(path.join(root, file)) })),
  sources: files,
};
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, `${JSON.stringify(manifest, null, 2)}\n`);
console.log(path.relative(root, output));
