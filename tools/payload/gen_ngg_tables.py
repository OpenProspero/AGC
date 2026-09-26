#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
"""Emit the NGG draw program for a payload from a fixture's compiler output.

The compiler is a build-time producer (see docs/shader-toolchain.md): this
script only reads the raw code and the metadata JSON that the pinned
opengnm-psbc checkout wrote for a fixture, and prints a C header holding

  - the machine code of both stages, and
  - the register tables the NGG draw encoder takes: the vertex stage's
    context and ES/GS shader registers, the linkage writes, and the GS
    user-data block with the LDS layout slot.

Nothing is invented: every offset and value comes from the metadata. The
header records the SHA-256 of both inputs so a console log names exactly
which compiler run produced the program it executed.

usage: gen_ngg_tables.py <vert.bin> <vert.metadata.json> <frag.bin>
                         <frag.metadata.json> <output.h>
"""

import hashlib
import json
import sys
from pathlib import Path

# SPI_SHADER_USER_DATA_GS_0: the NGG vertex stage's user-data block.
GS_USER_DATA_0 = 140
USER_DATA_DWORDS = 4
# The dwords the runtime fills for a one-vertex auto-indexed draw.
RUNTIME_ZERO_FIELDS = (
    "vertex_buffer_table_user_data_dword",
    "descriptor_set0_user_data_dword",
    "base_vertex_user_data_dword",
    "is_indexed_draw_user_data_dword",
    "start_instance_user_data_dword",
    "instance_id_bias_user_data_dword",
    "primitive_query_buffer_user_data_dword",
    "primitive_query_state_user_data_dword",
    "streamout_buffer_table_user_data_dword",
    "scratch_buffer_table_user_data_dword",
    "compute_grid_size_user_data_dword",
)


class Refusal(Exception):
    pass


def load(path):
    data = path.read_bytes()
    return data, hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise Refusal(message)


def registers(metadata, key, stage):
    entries = metadata.get(key)
    require(isinstance(entries, list) and entries,
            f"{stage}: metadata has no {key}")
    offsets = []
    values = []
    for entry in entries:
        require(isinstance(entry, dict) and "offset" in entry and "value" in entry,
                f"{stage}: {key} entry is not an offset/value pair")
        offsets.append(int(entry["offset"]))
        values.append(int(entry["value"]))
    require(all(0 <= value <= 0xFFFFFFFF for value in offsets + values),
            f"{stage}: {key} value out of 32-bit range")
    return offsets, values


def array(name, values, fmt="0x{:08x}u"):
    body = ", ".join(fmt.format(value) for value in values)
    return f"static const uint32_t {name}[] = {{ {body} }};\n"


def code_array(name, data):
    lines = []
    for start in range(0, len(data), 12):
        chunk = data[start:start + 12]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return (f"#define {name}_BYTES {len(data)}u\n"
            f"static const uint8_t {name}[{name}_BYTES] = {{\n"
            + "\n".join(lines) + "\n};\n")


def validate_ngg(metadata, stage):
    require(metadata.get("version") == 14, f"{stage}: unexpected metadata version")
    require(metadata.get("target") == 2, f"{stage}: fixture is not a PS5 target")
    require(metadata.get("hardware_stage") == 3,
            f"{stage}: fixture is not NGG (hardware_stage != 3)")
    linkage = metadata.get("linkage")
    require(isinstance(linkage, dict), f"{stage}: metadata has no linkage")
    require(linkage["stages_en"]["offset"] == 725,
            f"{stage}: linkage stages_en is not VGT_SHADER_STAGES_EN")
    require(linkage["ge_cntl"]["offset"] == 603,
            f"{stage}: linkage ge_cntl is not GE_CNTL")
    require(linkage["user_vgpr_en"]["offset"] == 610,
            f"{stage}: linkage user_vgpr_en is not SPI_SHADER_USER_VGPR_EN")
    require(linkage["stages_en"]["value"] & (1 << 13),
            f"{stage}: NGG stages_en does not carry PRIMGEN_EN")
    offsets, _ = registers(metadata, "shader_registers", stage)
    require(offsets[0] == 200 and offsets[1] == 201,
            f"{stage}: NGG shader registers do not start at the ES PGM pair")
    return linkage


def main():
    if len(sys.argv) != 6:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    vert_bin = Path(sys.argv[1])
    vert_meta_path = Path(sys.argv[2])
    frag_bin = Path(sys.argv[3])
    frag_meta_path = Path(sys.argv[4])
    output = Path(sys.argv[5])
    for path in (vert_bin, vert_meta_path, frag_bin, frag_meta_path):
        if not path.is_file():
            print(f"generator refused: missing {path}", file=sys.stderr)
            return 1

    vert_code, vert_code_sha = load(vert_bin)
    frag_code, frag_code_sha = load(frag_bin)
    vert_meta_file, vert_meta_sha = load(vert_meta_path)
    frag_meta_file, frag_meta_sha = load(frag_meta_path)
    vert_meta = json.loads(vert_meta_file.decode("utf-8"))
    frag_meta = json.loads(frag_meta_file.decode("utf-8"))

    try:
        linkage = validate_ngg(vert_meta, "vert")
        require(frag_meta.get("version") == 14, "frag: unexpected metadata version")
        require(frag_meta.get("target") == 2, "frag: fixture is not a PS5 target")

        vert_ctx_off, vert_ctx_val = registers(vert_meta, "context_registers", "vert")
        vert_sh_off, vert_sh_val = registers(vert_meta, "shader_registers", "vert")
        frag_ctx_off, frag_ctx_val = registers(frag_meta, "context_registers", "frag")
        frag_sh_off, frag_sh_val = registers(frag_meta, "shader_registers", "frag")

        layout_entry = vert_meta.get("ngg_lds_layout")
        require(isinstance(layout_entry, dict) and
                isinstance(layout_entry.get("value"), int) and
                isinstance(layout_entry.get("user_data_dword"), int),
                "vert: metadata has no ngg_lds_layout pair")
        layout = layout_entry["value"]
        layout_dword = layout_entry["user_data_dword"]
        require(0 <= layout_dword < USER_DATA_DWORDS,
                "vert: ngg_lds_layout user-data dword is out of range")

        user_data_off = [GS_USER_DATA_0 + i for i in range(USER_DATA_DWORDS)]
        user_data_val = [0] * USER_DATA_DWORDS
        for field in RUNTIME_ZERO_FIELDS:
            dword = vert_meta.get(field)
            require(dword is None or (isinstance(dword, int) and
                                      0 <= dword < USER_DATA_DWORDS),
                    f"vert: {field} is out of range")
        user_data_val[layout_dword] = layout

        link_off = [linkage["ge_cntl"]["offset"], linkage["stages_en"]["offset"],
                    linkage["user_vgpr_en"]["offset"]]
        link_val = [linkage["ge_cntl"]["value"], linkage["stages_en"]["value"],
                    linkage["user_vgpr_en"]["value"]]
    except Refusal as error:
        print(f"generator refused: {error}", file=sys.stderr)
        return 1

    header = f"""/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generated by tools/payload/gen_ngg_tables.py - do not edit.
 *
 * The NGG vertex fixture was produced locally by the pinned opengnm-psbc
 * sources (see tools/build-pinned-psbc.sh for the pinned workflow; this run
 * used a macOS host toolchain, so its digests are not the CI artifact's).
 * Provenance is recorded here and repeated in the payload's console log:
 *
 *   vert  {vert_bin.name}  sha256 {vert_code_sha}
 *   vert  {vert_meta_path.name}  sha256 {vert_meta_sha}
 *   frag  {frag_bin.name}  sha256 {frag_code_sha}
 *   frag  {frag_meta_path.name}  sha256 {frag_meta_sha}
 */

#ifndef OPENAGC_NGG_SMOKE_TABLES_H
#define OPENAGC_NGG_SMOKE_TABLES_H

#include <stdint.h>

{code_array("openagc_ngg_vert_code", vert_code)}
{code_array("openagc_ngg_frag_code", frag_code)}
#define OPENAGC_NGG_VERTEX_CONTEXT_COUNT {len(vert_ctx_off)}u
{array("openagc_ngg_vertex_context_offsets", vert_ctx_off)}
{array("openagc_ngg_vertex_context_values", vert_ctx_val)}
#define OPENAGC_NGG_VERTEX_SHADER_COUNT {len(vert_sh_off)}u
{array("openagc_ngg_vertex_shader_offsets", vert_sh_off)}
{array("openagc_ngg_vertex_shader_values", vert_sh_val)}
#define OPENAGC_NGG_LINKAGE_COUNT {len(link_off)}u
{array("openagc_ngg_linkage_offsets", link_off)}
{array("openagc_ngg_linkage_values", link_val)}
#define OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT {len(frag_ctx_off)}u
{array("openagc_ngg_fragment_context_offsets", frag_ctx_off)}
{array("openagc_ngg_fragment_context_values", frag_ctx_val)}
#define OPENAGC_NGG_FRAGMENT_SHADER_COUNT {len(frag_sh_off)}u
{array("openagc_ngg_fragment_shader_offsets", frag_sh_off)}
{array("openagc_ngg_fragment_shader_values", frag_sh_val)}
#define OPENAGC_NGG_USER_DATA_COUNT {USER_DATA_DWORDS}u
{array("openagc_ngg_user_data_offsets", user_data_off)}
{array("openagc_ngg_user_data_values", user_data_val)}
#define OPENAGC_NGG_USER_DATA_LAYOUT_SLOT {layout_dword}u
#define OPENAGC_NGG_USER_DATA_LAYOUT {layout}u

#endif /* OPENAGC_NGG_SMOKE_TABLES_H */
"""
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(header, encoding="utf-8")
    print(f"wrote {output} (vert {len(vert_code)}B, frag {len(frag_code)}B, "
          f"layouts {len(vert_ctx_off)}/{len(vert_sh_off)}/{len(frag_ctx_off)}/"
          f"{len(frag_sh_off)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
