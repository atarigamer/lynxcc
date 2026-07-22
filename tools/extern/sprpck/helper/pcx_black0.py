#!/usr/bin/env python3
"""
pcx_black0.py -- Reorders the palette of 8bit/1plane PCX files so that the
blackest color ends up at palette index 0 (pixel data is remapped
accordingly). Purpose: sprpck can then use -e0 (edgePen=0) to trim
"black" pixels at the end of a line.

Only supports 8bit/1plane PCX (the default case used by sprpck). Other
PCX variants are skipped and reported as errors.

Usage:
  # single file, result written next to the original with a _black0 suffix
  python3 pcx_black0.py image.pcx

  # whole directory, results written to a separate output directory
  python3 pcx_black0.py --outdir out/ *.pcx

  # overwrite originals in place (careful!)
  python3 pcx_black0.py --inplace *.pcx

  # force a specific target index instead of the "blackest color", e.g.
  # if you know index 5 is actually the background color:
  python3 pcx_black0.py --force-index 5 image.pcx
"""

import argparse
import glob
import os
import sys


def load_pcx(path):
    data = open(path, "rb").read()
    if data[0] != 10:
        raise ValueError("Not a valid PCX file (wrong manufacturer byte)")
    bpp = data[3]
    nplanes = data[65]
    if not (bpp == 8 and nplanes == 1):
        raise ValueError(f"Only 8bit/1plane PCX supported (this file: {bpp}bit/{nplanes}plane)")

    header = bytearray(data[:128])
    xmin = int.from_bytes(header[4:6], "little")
    ymin = int.from_bytes(header[6:8], "little")
    xmax = int.from_bytes(header[8:10], "little")
    ymax = int.from_bytes(header[10:12], "little")
    w = xmax - xmin + 1
    h = ymax - ymin + 1
    bytes_per_line = int.from_bytes(header[66:68], "little")

    if len(data) < 769 or data[-769] != 0x0C:
        raise ValueError("No 256-color palette (0x0C marker) found at end of file")
    palette = bytearray(data[-768:])

    pos = 128
    plane_bytes = []
    total_needed = bytes_per_line * h
    while len(plane_bytes) < total_needed:
        b = data[pos]
        pos += 1
        if (b & 0xC0) == 0xC0:
            count = b & 0x3F
            val = data[pos]
            pos += 1
        else:
            count = 1
            val = b
        plane_bytes.extend([val] * count)

    raster = [plane_bytes[y * bytes_per_line: y * bytes_per_line + w] for y in range(h)]
    return header, raster, palette, w, h, bytes_per_line


def rle_encode_row(row):
    out = bytearray()
    i = 0
    n = len(row)
    while i < n:
        val = row[i]
        run = 1
        while i + run < n and row[i + run] == val and run < 63:
            run += 1
        if run > 1 or (val & 0xC0) == 0xC0:
            out.append(0xC0 | run)
            out.append(val)
        else:
            out.append(val)
        i += run
    return out


def save_pcx(path, header, raster, palette, bytes_per_line):
    out = bytearray(header)
    for row in raster:
        if bytes_per_line > len(row):
            padded = list(row) + [row[-1]] * (bytes_per_line - len(row))
        else:
            padded = row
        out.extend(rle_encode_row(padded))
    out.append(0x0C)
    out.extend(palette)
    with open(path, "wb") as f:
        f.write(out)


def find_blackest_index(palette, n_colors=256):
    """Finds the palette index with the smallest distance to (0,0,0)."""
    best_idx = 0
    best_dist = None
    for i in range(n_colors):
        r, g, b = palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]
        dist = r * r + g * g + b * b
        if best_dist is None or dist < best_dist:
            best_dist = dist
            best_idx = i
    return best_idx, best_dist


def process_file(path, outpath, force_index=None):
    header, raster, palette, w, h, bpl = load_pcx(path)

    if force_index is not None:
        black_idx = force_index
        r, g, b = palette[black_idx*3], palette[black_idx*3+1], palette[black_idx*3+2]
    else:
        black_idx, dist2 = find_blackest_index(palette)
        r, g, b = palette[black_idx*3], palette[black_idx*3+1], palette[black_idx*3+2]
        if dist2 != 0:
            print(f"  Warning: blackest color is not exactly (0,0,0), "
                  f"but ({r},{g},{b}) at index {black_idx}", file=sys.stderr)

    if black_idx == 0:
        print(f"  Index 0 is already the closest-to-(0,0,0) color ({r},{g},{b}) -- copied unchanged")
        save_pcx(outpath, header, raster, palette, bpl)
        return

    # swap palette entries
    p = bytearray(palette)
    p[0:3], p[black_idx*3:black_idx*3+3] = p[black_idx*3:black_idx*3+3], p[0:3]

    # swap pixel indices (0 <-> black_idx)
    def remap(v):
        if v == 0:
            return black_idx
        if v == black_idx:
            return 0
        return v

    new_raster = [[remap(v) for v in row] for row in raster]

    save_pcx(outpath, header, new_raster, p, bpl)

    n_black_now = sum(row.count(0) for row in new_raster)
    print(f"  Swapped index 0 <-> index {black_idx} (color was ({r},{g},{b})), "
          f"{n_black_now} pixels now at index 0")


def main():
    ap = argparse.ArgumentParser(description="Reorder PCX palette so black ends up at index 0")
    ap.add_argument("files", nargs="+", help="PCX files (glob patterns are expanded)")
    ap.add_argument("--outdir", help="Output directory (default: _black0 suffix next to original)")
    ap.add_argument("--inplace", action="store_true", help="Overwrite original files")
    ap.add_argument("--force-index", type=int, default=None,
                     help="Use this palette index instead of the auto-detected blackest color")
    args = ap.parse_args()

    if args.outdir:
        os.makedirs(args.outdir, exist_ok=True)

    all_files = []
    for pattern in args.files:
        matches = glob.glob(pattern)
        all_files.extend(matches if matches else [pattern])

    if not all_files:
        print("No matching files found.", file=sys.stderr)
        sys.exit(1)

    n_ok = 0
    n_fail = 0
    for path in all_files:
        print(f"{path}:")
        if args.inplace:
            outpath = path
        elif args.outdir:
            outpath = os.path.join(args.outdir, os.path.basename(path))
        else:
            base, ext = os.path.splitext(path)
            outpath = f"{base}_black0{ext}"

        try:
            process_file(path, outpath, force_index=args.force_index)
            n_ok += 1
        except Exception as e:
            print(f"  ERROR: {e}", file=sys.stderr)
            n_fail += 1

    print(f"\nDone: {n_ok} processed, {n_fail} failed.")
    if n_fail:
        sys.exit(1)


if __name__ == "__main__":
    main()
