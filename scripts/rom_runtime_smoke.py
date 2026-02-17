#!/usr/bin/env python3
"""Run headless runtime smoke checks for NES ROMs and emit an auto-filled CSV.

This script compiles a tiny core-only NES runner (no SDL), runs each ROM for a
fixed number of frames, and records simple runtime health signals:
- load/reset success
- CPU jammed flag
- framebuffer hash progression

It complements scripts/rom_regression_matrix.py by auto-filling machine-derived
columns while keeping manual triage columns for humans.
"""

from __future__ import annotations

import argparse
import csv
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List

SUPPORTED_MAPPERS = {0, 1, 2}


@dataclass
class RomEntry:
    path: Path
    mapper: int
    prg_banks_16k: int
    chr_banks_8k: int
    mirroring: str


def parse_ines_header(path: Path) -> RomEntry:
    with path.open("rb") as f:
        h = f.read(16)

    if len(h) < 16 or h[0:4] != b"NES\x1a":
        raise ValueError("not an iNES file")

    prg_banks_16k = h[4]
    chr_banks_8k = h[5]
    flags6 = h[6]
    flags7 = h[7]
    mapper = ((flags7 >> 4) << 4) | (flags6 >> 4)

    if flags6 & 0x08:
        mirroring = "four-screen"
    elif flags6 & 0x01:
        mirroring = "vertical"
    else:
        mirroring = "horizontal"

    return RomEntry(
        path=path,
        mapper=mapper,
        prg_banks_16k=prg_banks_16k,
        chr_banks_8k=chr_banks_8k,
        mirroring=mirroring,
    )


def iter_roms(root: Path) -> Iterable[RomEntry]:
    for rom in sorted(root.rglob("*.nes")):
        try:
            yield parse_ines_header(rom)
        except ValueError:
            continue


def read_existing_manual_columns(path: Path) -> Dict[str, Dict[str, str]]:
    if not path.exists():
        return {}

    out: Dict[str, Dict[str, str]] = {}
    with path.open("r", newline="", encoding="utf-8") as f:
        r = csv.DictReader(f)
        for row in r:
            rom = row.get("rom", "")
            if not rom:
                continue
            out[rom] = {
                "boot": row.get("boot", ""),
                "title_stable": row.get("title_stable", ""),
                "input_ok": row.get("input_ok", ""),
                "notes": row.get("notes", ""),
            }
    return out


def build_runner(binary: Path, cc: str) -> None:
    src = binary.with_suffix(".c")
    src.write_text(
        r'''
#include "nes/nes.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t fnv1a64(const uint32_t* data, size_t n_words)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n_words; i++) {
        uint32_t v = data[i];
        for (int b = 0; b < 4; b++) {
            uint8_t x = (uint8_t)(v & 0xFFu);
            h ^= x;
            h *= 1099511628211ull;
            v >>= 8;
        }
    }
    return h;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s ROM FRAMES\n", argv[0]);
        return 2;
    }

    const char* rom = argv[1];
    int frames = atoi(argv[2]);
    if (frames <= 0) frames = 120;

    Nes n;
    if (!NES_Init(&n)) {
        printf("status=init_fail\n");
        return 1;
    }

    if (!NES_LoadROM(&n, rom)) {
        printf("status=load_fail\n");
        NES_Destroy(&n);
        return 1;
    }

    NES_Reset(&n);

    uint64_t first_hash = 0;
    uint64_t last_hash = 0;
    int changed_frames = 0;
    int jammed = 0;

    uint64_t* uniq = (uint64_t*)calloc((size_t)frames, sizeof(uint64_t));
    int uniq_count = 0;

    for (int i = 0; i < frames; i++) {
        NES_RunFrame(&n);

        if (n.cpu.jammed) jammed = 1;

        uint64_t h = fnv1a64(NES_Framebuffer(&n), (size_t)(NES_FB_W * NES_FB_H));
        if (i == 0) first_hash = h;
        if (h != first_hash) changed_frames++;
        last_hash = h;

        int seen = 0;
        for (int j = 0; j < uniq_count; j++) {
            if (uniq[j] == h) { seen = 1; break; }
        }
        if (!seen && uniq_count < frames) {
            uniq[uniq_count++] = h;
        }
    }

    printf("status=ok\n");
    printf("jammed=%d\n", jammed);
    printf("frames=%d\n", frames);
    printf("changed_frames=%d\n", changed_frames);
    printf("unique_hashes=%d\n", uniq_count);
    printf("first_hash=%llu\n", (unsigned long long)first_hash);
    printf("last_hash=%llu\n", (unsigned long long)last_hash);

    free(uniq);
    NES_Destroy(&n);
    return 0;
}
''',
        encoding="utf-8",
    )

    cmd = [
        cc,
        "-std=c11",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Iinclude",
        str(src),
        "src/nes/nes.c",
        "src/nes/bus.c",
        "src/nes/cart.c",
        "src/nes/mapper.c",
        "src/nes/mapper_nrom.c",
        "src/nes/mapper_mmc1.c",
        "src/nes/mapper_uxrom.c",
        "src/nes/ines.c",
        "src/nes/util/file.c",
        "src/nes/ppu/ppu2c02.c",
        "src/nes/apu/apu2a03.c",
        "src/nes/cpu/cpu6502.c",
        "src/nes/cpu/cpu_tables.c",
        "-o",
        str(binary),
    ]
    subprocess.run(cmd, check=True)


def parse_kv(stdout: str) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for line in stdout.splitlines():
        if "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def run_one(binary: Path, rom: Path, frames: int, timeout_s: int) -> Dict[str, str]:
    p = subprocess.run(
        [str(binary), str(rom), str(frames)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout_s,
    )
    if p.returncode != 0:
        return {
            "auto_boot": "no",
            "auto_title_stable": "no",
            "auto_input_ok": "unknown",
            "auto_notes": "runner_error",
            "auto_changed_frames": "0",
            "auto_unique_hashes": "0",
            "auto_jammed": "unknown",
        }

    kv = parse_kv(p.stdout)
    status = kv.get("status", "")
    jammed = kv.get("jammed", "1")
    changed = int(kv.get("changed_frames", "0"))
    uniq = int(kv.get("unique_hashes", "0"))

    boot = "yes" if status == "ok" else "no"
    # Heuristic only: some change + at least two distinct frame hashes.
    title_stable = "yes" if changed > 0 and uniq >= 2 and jammed == "0" else "no"

    note_parts: List[str] = []
    if jammed != "0":
        note_parts.append("cpu_jammed")
    if changed == 0:
        note_parts.append("no_video_change")
    if uniq <= 1:
        note_parts.append("frozen_frame_hash")
    if not note_parts:
        note_parts.append("ok")

    return {
        "auto_boot": boot,
        "auto_title_stable": title_stable,
        "auto_input_ok": "unknown",
        "auto_notes": ";".join(note_parts),
        "auto_changed_frames": str(changed),
        "auto_unique_hashes": str(uniq),
        "auto_jammed": "yes" if jammed != "0" else "no",
    }


def write_csv(
    rows: List[RomEntry],
    runtime: Dict[str, Dict[str, str]],
    out_csv: Path,
    root: Path,
    existing: Dict[str, Dict[str, str]],
) -> None:
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "rom",
            "mapper",
            "supported_now",
            "prg_banks_16k",
            "chr_banks_8k",
            "mirroring",
            "auto_boot",
            "auto_title_stable",
            "auto_input_ok",
            "auto_jammed",
            "auto_changed_frames",
            "auto_unique_hashes",
            "auto_notes",
            "boot",
            "title_stable",
            "input_ok",
            "notes",
        ])

        for r in rows:
            key = str(r.path.relative_to(root))
            rt = runtime.get(key, {})
            ex = existing.get(key, {})
            w.writerow([
                key,
                r.mapper,
                "yes" if r.mapper in SUPPORTED_MAPPERS else "no",
                r.prg_banks_16k,
                r.chr_banks_8k,
                r.mirroring,
                rt.get("auto_boot", "no"),
                rt.get("auto_title_stable", "no"),
                rt.get("auto_input_ok", "unknown"),
                rt.get("auto_jammed", "unknown"),
                rt.get("auto_changed_frames", "0"),
                rt.get("auto_unique_hashes", "0"),
                rt.get("auto_notes", "not_run"),
                ex.get("boot", ""),
                ex.get("title_stable", ""),
                ex.get("input_ok", ""),
                ex.get("notes", ""),
            ])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom_dir", type=Path, help="Directory containing .nes files")
    ap.add_argument("--frames", type=int, default=120, help="Frames per ROM (default: 120)")
    ap.add_argument("--timeout", type=int, default=20, help="Per-ROM timeout seconds (default: 20)")
    ap.add_argument("--cc", default=os.environ.get("CC", "cc"), help="C compiler (default: $CC or cc)")
    ap.add_argument(
        "--out-csv",
        type=Path,
        default=Path("tests/rom_runtime_matrix.csv"),
        help="Output CSV path (default: tests/rom_runtime_matrix.csv)",
    )
    ap.add_argument(
        "--existing-csv",
        type=Path,
        default=Path("tests/rom_matrix.csv"),
        help="Existing manual CSV to carry manual columns from (default: tests/rom_matrix.csv)",
    )
    args = ap.parse_args()

    if not args.rom_dir.exists() or not args.rom_dir.is_dir():
        raise SystemExit(f"ROM directory not found: {args.rom_dir}")

    rows = list(iter_roms(args.rom_dir))
    if not rows:
        raise SystemExit("No valid .nes ROM files found")

    existing = read_existing_manual_columns(args.existing_csv)

    runtime: Dict[str, Dict[str, str]] = {}
    with tempfile.TemporaryDirectory(prefix="nes_smoke_") as td:
        binary = Path(td) / "rom_smoke_runner"
        build_runner(binary, args.cc)

        for r in rows:
            key = str(r.path.relative_to(args.rom_dir))
            if r.mapper not in SUPPORTED_MAPPERS:
                runtime[key] = {
                    "auto_boot": "no",
                    "auto_title_stable": "no",
                    "auto_input_ok": "unknown",
                    "auto_jammed": "unknown",
                    "auto_changed_frames": "0",
                    "auto_unique_hashes": "0",
                    "auto_notes": "unsupported_mapper",
                }
                continue

            try:
                runtime[key] = run_one(binary, r.path, args.frames, args.timeout)
            except subprocess.TimeoutExpired:
                runtime[key] = {
                    "auto_boot": "no",
                    "auto_title_stable": "no",
                    "auto_input_ok": "unknown",
                    "auto_jammed": "unknown",
                    "auto_changed_frames": "0",
                    "auto_unique_hashes": "0",
                    "auto_notes": "timeout",
                }

    write_csv(rows, runtime, args.out_csv, args.rom_dir, existing)
    print(f"Wrote runtime matrix for {len(rows)} ROMs -> {args.out_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
