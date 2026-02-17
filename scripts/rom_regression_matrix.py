#!/usr/bin/env python3
"""Generate an NES ROM compatibility matrix for mapper support and manual status tracking.

This tool scans a directory for .nes files, parses iNES headers, and emits:
- a CSV matrix (for manual pass/fail notes), and
- an optional Markdown summary highlighting unsupported mappers.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List

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


def write_csv(rows: List[RomEntry], out_csv: Path, root: Path) -> None:
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
            "boot",
            "title_stable",
            "input_ok",
            "notes",
        ])
        for r in rows:
            w.writerow([
                str(r.path.relative_to(root)),
                r.mapper,
                "yes" if r.mapper in SUPPORTED_MAPPERS else "no",
                r.prg_banks_16k,
                r.chr_banks_8k,
                r.mirroring,
                "",
                "",
                "",
                "",
            ])


def write_markdown(rows: List[RomEntry], out_md: Path, root: Path) -> None:
    out_md.parent.mkdir(parents=True, exist_ok=True)
    total = len(rows)
    supported = sum(1 for r in rows if r.mapper in SUPPORTED_MAPPERS)
    unsupported = total - supported

    with out_md.open("w", encoding="utf-8") as f:
        f.write("# ROM Regression Matrix Summary\n\n")
        f.write(f"- Total ROMs scanned: **{total}**\n")
        f.write(f"- Supported mappers (0/1/2): **{supported}**\n")
        f.write(f"- Unsupported mapper ROMs: **{unsupported}**\n\n")

        f.write("## Unsupported Mapper ROMs\n\n")
        if unsupported == 0:
            f.write("All scanned ROMs are currently mapper-supported.\n")
        else:
            f.write("| ROM | Mapper |\n|---|---:|\n")
            for r in rows:
                if r.mapper not in SUPPORTED_MAPPERS:
                    f.write(f"| `{str(r.path.relative_to(root))}` | {r.mapper} |\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom_dir", type=Path, help="Directory containing .nes files")
    ap.add_argument(
        "--csv",
        type=Path,
        default=Path("tests/rom_matrix.csv"),
        help="Output CSV path (default: tests/rom_matrix.csv)",
    )
    ap.add_argument(
        "--md",
        type=Path,
        default=Path("tests/rom_matrix.md"),
        help="Output markdown summary path (default: tests/rom_matrix.md)",
    )
    args = ap.parse_args()

    if not args.rom_dir.exists() or not args.rom_dir.is_dir():
        raise SystemExit(f"ROM directory not found: {args.rom_dir}")

    rows = list(iter_roms(args.rom_dir))
    if not rows:
        raise SystemExit("No valid .nes ROM files found")

    write_csv(rows, args.csv, args.rom_dir)
    write_markdown(rows, args.md, args.rom_dir)
    print(f"Wrote {len(rows)} entries to {args.csv} and {args.md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
