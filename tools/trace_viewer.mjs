#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const args = process.argv.slice(2);

function usage() {
  console.error("usage: trace_viewer.mjs --trace-json FILE [--csv FILE] [--html FILE]");
  process.exit(2);
}

function readArg(name) {
  const index = args.indexOf(name);
  if (index < 0) return null;
  if (index + 1 >= args.length) usage();
  return args[index + 1];
}

const input = readArg("--trace-json");
const csvOut = readArg("--csv");
const htmlOut = readArg("--html");

if (!input || (!csvOut && !htmlOut)) {
  usage();
}

const raw = fs.readFileSync(input, "utf8");
const parsed = JSON.parse(raw);
const entries = Array.isArray(parsed) ? parsed : parsed.entries;

if (!Array.isArray(entries)) {
  throw new Error("trace JSON must be an array or an object with entries[]");
}

function norm(entry, index) {
  return {
    index,
    sequence: Number(entry.sequence ?? index),
    timestamp: Number(entry.timestamp ?? entry.time ?? 0),
    event: String(entry.event ?? entry.event_name ?? entry.type ?? ""),
    arg0: Number(entry.arg0 ?? entry.d0 ?? 0),
    arg1: Number(entry.arg1 ?? entry.d1 ?? 0),
  };
}

const rows = entries.map(norm);

function csvEscape(value) {
  const text = String(value);
  if (/[",\n]/.test(text)) {
    return `"${text.replace(/"/g, '""')}"`;
  }
  return text;
}

if (csvOut) {
  const csv = [
    "index,sequence,timestamp,event,arg0,arg1",
    ...rows.map((row) =>
      [row.index, row.sequence, row.timestamp, row.event, row.arg0, row.arg1]
        .map(csvEscape)
        .join(",")
    ),
  ].join("\n");
  fs.mkdirSync(path.dirname(csvOut), { recursive: true });
  fs.writeFileSync(csvOut, `${csv}\n`);
}

if (htmlOut) {
  const maxTime = Math.max(1, ...rows.map((row) => row.timestamp));
  const tableRows = rows.map((row) => {
    const width = Math.max(1, Math.round((row.timestamp / maxTime) * 100));
    return `<tr><td>${row.index}</td><td>${row.sequence}</td><td>${row.timestamp}</td><td>${row.event}</td><td>${row.arg0}</td><td>${row.arg1}</td><td><div class="bar" style="width:${width}%"></div></td></tr>`;
  }).join("\n");

  const html = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>AIXOS Trace Viewer</title>
<style>
body{font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:24px;color:#17202a;background:#fafafa}
h1{font-size:24px;margin:0 0 12px}
table{border-collapse:collapse;width:100%;background:#fff}
th,td{border:1px solid #d5d8dc;padding:6px 8px;font-size:13px;text-align:left}
th{background:#eef2f5}
.bar{height:10px;background:#2f6f9f}
</style>
</head>
<body>
<h1>AIXOS Trace Viewer</h1>
<p>Entries: ${rows.length}</p>
<table>
<thead><tr><th>#</th><th>sequence</th><th>timestamp</th><th>event</th><th>arg0</th><th>arg1</th><th>timeline</th></tr></thead>
<tbody>
${tableRows}
</tbody>
</table>
</body>
</html>
`;
  fs.mkdirSync(path.dirname(htmlOut), { recursive: true });
  fs.writeFileSync(htmlOut, html);
}

