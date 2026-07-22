<!--
SPDX-License-Identifier: CC-BY-4.0
Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
-->

# Vendored third-party tools (`tools/extern/`)

This directory holds external tools that ship with **lynxcc** but are **not**
authored by this project. Each subdirectory is a pristine copy of an upstream
repository, brought in as a **git subtree** and built alongside the rest of
`tools/`. The full rationale and build design is
`design/LYNX_EXTERN_TOOLS_DESIGN.md`; this file is the quick reference and the
provenance registry.

## Rules

1. **Never modify vendored code.** Everything under `extern/<tool>/` must stay
   byte-identical to upstream. No SPDX tags, no reformatting, no local patches,
   and no metadata files added *inside* the vendored tree — a `git subtree pull`
   would clobber them, and edits would break the clean upstream link.
2. **Project-owned glue lives outside the vendored prefix.** This `README.md`,
   the build wiring in `tools/Makefile` (`EXTERN_PROGS` / `EXTERN_CXX_PROGS` and
   their templates), and the licence registry in `doc/licenses.html` are the
   project's files and are edited freely. They reference the vendored sources;
   they never live among them.
3. **Build, don't fork.** Because upstream code can't be edited, the tool is
   compiled from its own source layout via a per-tool source glob and any
   upstream-required defines, with strict warnings disabled (`EXTERN_CFLAGS`, or
   `EXTERN_CXXFLAGS` for C++ tools) — we do not lint code we do not own. The
   binary installs into the shared root `bin/`, exactly like the project's own
   tools.
4. **Respect the upstream licence.** Each tool keeps its own licence, recorded
   below and in `doc/licenses.html`. Vendored tools are **not** relicensed under
   the SDK's MPL-2.0. Where a tool ships **no** licence at all, that fact is
   recorded explicitly — the code is used as-is under the author's reserved
   rights, pending an explicit grant, and is flagged in both places.

## Updating a vendored tool

Run from the repository root (needs network to the upstream forge — note the
sandbox proxy blocks Codeberg, so pulls run on a developer machine):

```
git subtree pull --prefix tools/extern/<tool> <url> <branch> --squash
```

After a pull, update the pinned commit in the table below and re-run a full
rebuild.

## Registry

| Tool | Upstream | Branch | Pinned commit | Licence | Build |
|------|----------|--------|---------------|---------|-------|
| `sprpck` | https://codeberg.org/42Bastian/sprpck | `main` | `ca574db663` | Apache-2.0 | `EXTERN_PROGS`, sources `extern/sprpck/src/*.c`, `-DUNIX`; → `bin/sprpck` |
| `lynxdir` | https://github.com/bspruck/lynxdir | `master` | `3e46f9610b` | **None declared** (all rights reserved, © 2010–2017 Björn Spruck) | `EXTERN_CXX_PROGS`, sources `extern/lynxdir/*.cpp`; → `bin/lynxdir` |

### sprpck — Lynx Sprite Packer

Converts images (PCX, BMP, PI1, raw, SPS) into Atari Lynx sprite data, with
packing/literal optimisation, action points, tiling, palette export (C / ASM /
LYXASS) and cc65-object output (`-p0`). Copyright © 1997–2021 42Bastian Schick
and Matthias Domin, with contributions from Karri Kaksonen and LordKraken;
licensed under Apache-2.0 (see `sprpck/LICENSE`).

It overlaps in purpose with the SDK's own `sp65` sprite converter but is
complementary: `sprpck` adds action points, PI1/BMP input, batch mode and LYXASS
palette output that `sp65`'s `lynx-sprite` mode does not cover.

### lynxdir — Lynx ROM Builder

Assembles a bootable Lynx cartridge ROM from a list of files plus an encrypted
loader, driven by a `.mak` config file. It supports EPYX, BLL and NewMini
directory layouts, 512/1024/2048-byte blocks (128k/256k/512k ROMs), AUDIN/BANK2
bank switching, title-picture handling and `.lnx`/`.lyx` output. Copyright ©
2010–2017 Björn Spruck. Version 1.9. Written in C++ (`lynxdir.cpp`,
`lynxrom.cpp`), so it builds through the Makefile's `EXTERN_CXX_PROGS` path
(`$(CXX)`) rather than the C `EXTERN_PROGS` path.

It overlaps in purpose with the SDK's own `lnx` tool (`lnx bll` / `.lnx`
create/patch) but comes from the wider Lynx homebrew ecosystem and covers loader
layouts and `.mak`-driven multi-file ROM assembly that `lnx` does not; it is
vendored as a standalone convenience tool and is not wired into `cl65` or any
example.

> **Licence caveat.** lynxdir ships **no licence file** — the upstream repo
> declares only `(c) Björn Spruck 2010-2017`, i.e. all rights reserved by
> default. It is vendored **unmodified** and used as-is pending an explicit
> licence grant from the author; it is **not** relicensed under the SDK's
> MPL-2.0. This is recorded in `doc/licenses.html` §4.4. If you redistribute the
> SDK, resolve this with upstream first.
