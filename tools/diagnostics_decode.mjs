#!/usr/bin/env node

import fs from "node:fs";

const args = process.argv.slice(2);

function usage() {
  console.error("usage: diagnostics_decode.mjs --crash FILE | --trace FILE");
  process.exit(2);
}

function hex(value) {
  return `0x${value.toString(16).padStart(8, "0")}`;
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function decodeCrash(path) {
  const data = fs.readFileSync(path);
  if (data.length < 60) {
    throw new Error("crash record must contain at least 60 bytes");
  }
  const u32 = (offset) => data.readUInt32LE(offset);
  const record = {
    magic: hex(u32(0)),
    version: data.readUInt16LE(4),
    size: data.readUInt16LE(6),
    sequence: u32(8),
    build_id: hex(u32(12)),
    architecture: u32(16),
    reason: u32(20),
    program_counter: hex(u32(24)),
    fault_address: hex(u32(28)),
    stack_pointer: hex(u32(32)),
    task_handle: hex(u32(36)),
    tick: u32(40),
    fault_status: hex(u32(44)),
    fault_status2: hex(u32(48)),
    auxiliary: hex(u32(52)),
    crc32: hex(u32(56)),
  };
  record.computed_crc32 = hex(crc32(data.subarray(4, 56)));
  record.crc_valid = record.crc32 === record.computed_crc32;
  console.log(JSON.stringify(record, null, 2));
}

function decodeTrace(path) {
  const data = fs.readFileSync(path);
  if (data.length % 20 !== 0) {
    throw new Error("trace binary length must be a multiple of 20 bytes");
  }
  const entries = [];
  for (let offset = 0; offset < data.length; offset += 20) {
    entries.push({
      sequence: data.readUInt32LE(offset),
      timestamp: data.readUInt32LE(offset + 4),
      event: data.readUInt16LE(offset + 8),
      arg0: hex(data.readUInt32LE(offset + 12)),
      arg1: hex(data.readUInt32LE(offset + 16)),
    });
  }
  console.log(JSON.stringify(entries, null, 2));
}

if (args.length !== 2) usage();
if (args[0] === "--crash") decodeCrash(args[1]);
else if (args[0] === "--trace") decodeTrace(args[1]);
else usage();
