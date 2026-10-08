#!/usr/bin/env python3
"""Assemble a CHIP-8 source file into a ROM.

Usage: python3 games/assemble.py [source.asm] [output.ch8]
(defaults: the .asm next to this script, output with a .ch8 suffix)

Supported subset: labels, `.byte` data and the opcodes used by the
bundled game. Numbers are decimal or 0x-prefixed hex; comments start
with ';'.
"""
import re
import sys
from pathlib import Path

REGS = {f"V{i:X}": i for i in range(16)}


def number(tok, labels):
    if tok in labels:
        return labels[tok]
    return int(tok, 16) if tok.lower().startswith("0x") else int(tok)


def encode(stmt, labels):
    parts = stmt.replace(",", " ").split()
    op, args = parts[0].upper(), parts[1:]

    def reg(tok):
        if tok.upper() not in REGS:
            raise ValueError(f"expected register, got {tok!r} in {stmt!r}")
        return REGS[tok.upper()]

    def imm(tok):
        return number(tok, labels)

    if op == "CLS" and not args:
        return 0x00E0
    if op in ("JP", "CALL") and len(args) == 1:
        return (0x1000 if op == "JP" else 0x2000) | (imm(args[0]) & 0xFFF)
    if op in ("SE", "SNE") and len(args) == 2:
        x = reg(args[0])
        if args[1].upper() in REGS:
            base = 0x5000 if op == "SE" else 0x9000
            return base | (x << 8) | (reg(args[1]) << 4)
        base = 0x3000 if op == "SE" else 0x4000
        return base | (x << 8) | (imm(args[1]) & 0xFF)
    if op == "LD" and len(args) == 2:
        a, b = args
        if a.upper() == "I":
            return 0xA000 | (imm(b) & 0xFFF)
        if a.upper() == "DT":
            return 0xF015 | (reg(b) << 8)
        if a.upper() == "ST":
            return 0xF018 | (reg(b) << 8)
        if a.upper() == "F":
            return 0xF029 | (reg(b) << 8)
        x = reg(a)
        if b.upper() == "DT":
            return 0xF007 | (x << 8)
        if b == "[I]":
            return 0xF065 | (x << 8)
        if b.upper() in REGS:
            return 0x8000 | (x << 8) | (reg(b) << 4)
        return 0x6000 | (x << 8) | (imm(b) & 0xFF)
    if op == "ADD" and len(args) == 2:
        return 0x7000 | (reg(args[0]) << 8) | (imm(args[1]) & 0xFF)
    if op == "RND" and len(args) == 2:
        return 0xC000 | (reg(args[0]) << 8) | (imm(args[1]) & 0xFF)
    if op == "DRW" and len(args) == 3:
        return (0xD000 | (reg(args[0]) << 8) | (reg(args[1]) << 4) |
                (imm(args[2]) & 0xF))
    if op in ("SKP", "SKNP") and len(args) == 1:
        return (0xE09E if op == "SKP" else 0xE0A1) | (reg(args[0]) << 8)
    if op == "BCD" and len(args) == 1:
        return 0xF033 | (reg(args[0]) << 8)
    raise ValueError(f"cannot assemble {stmt!r}")


def assemble(text, origin=0x200):
    statements = []  # (address, kind, payload)
    labels = {}
    addr = origin
    for raw in text.splitlines():
        line = raw.split(";", 1)[0].strip()
        while line:
            m = re.match(r"^([A-Za-z_]\w*)\s*:\s*(.*)$", line)
            if not m:
                break
            labels[m.group(1)] = addr
            line = m.group(2).strip()
        if not line:
            continue
        if line.startswith(".byte"):
            values = line.replace(",", " ").split()[1:]
            statements.append((addr, "data", values))
            addr += len(values)
        else:
            statements.append((addr, "op", line))
            addr += 2

    rom = bytearray()
    for addr, kind, payload in statements:
        if kind == "data":
            for value in payload:
                rom.append(number(value, labels) & 0xFF)
        else:
            word = encode(payload, labels)
            rom.append((word >> 8) & 0xFF)
            rom.append(word & 0xFF)
    return bytes(rom)


def main():
    base = Path(__file__).resolve().parent
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else base / "catch.asm"
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else src.with_suffix(".ch8")
    rom = assemble(src.read_text())
    dst.write_bytes(rom)
    print(f"{dst}: {len(rom)} bytes")


if __name__ == "__main__":
    main()
