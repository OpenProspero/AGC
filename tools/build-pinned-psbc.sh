#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
set -Eeuo pipefail
export LC_ALL=C

fail() {
    printf 'OpenAGC PSBC build: %s\n' "$*" >&2
    exit 1
}

[[ -x /usr/bin/python3 ]] || fail 'Ubuntu system Python is required for pinned apt modules'
export PATH="/usr/bin:$PATH"

[[ $# -eq 2 ]] || fail 'expected WORK_DIRECTORY and ARTIFACT_DIRECTORY'
work_root=$1
artifact_dir=$2
[[ $work_root == /* && $artifact_dir == /* ]] ||
    fail 'both output paths must be absolute Linux paths'
[[ $work_root != "$artifact_dir" ]] || fail 'work and artifact paths must differ'
[[ $work_root != "$artifact_dir/"* && $artifact_dir != "$work_root/"* ]] ||
    fail 'work and artifact paths must not contain one another'
[[ ! -e $work_root && ! -e $artifact_dir ]] ||
    fail 'refusing to reuse an existing work or artifact directory'
jobs=${PSBC_JOBS:-2}
[[ $jobs =~ ^[1-4]$ ]] || fail 'PSBC_JOBS must be between 1 and 4'

for program in git python3 make gcc g++ glslangValidator spirv-val sha256sum xz; do
    command -v "$program" >/dev/null || fail "missing build prerequisite: $program"
done
python3 -c 'import mako, yaml, packaging' ||
    fail 'Python Mako, PyYAML and packaging are required'

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
[[ $work_root != "$repo_root"* && $artifact_dir != "$repo_root"* ]] ||
    fail 'build outputs must remain outside the OpenAGC checkout'
mkdir -p -- "$work_root"
upstream="$work_root/ps5-opengl"
smoke="$work_root/smoke"
mkdir -p -- "$smoke"

git clone --branch v0.3.0 --depth 1 --filter=blob:none \
    https://github.com/blackbearreloaded/ps5-opengl.git "$upstream"
python3 "$repo_root/tools/verify_pinned_psbc.py" release "$upstream"

(
    cd -- "$upstream"
    python3 tools/fetch-sources.py
    python3 tools/fetch-sources.py --verify-psbc
)
python3 "$repo_root/tools/verify_pinned_psbc.py" sources "$upstream"

(
    cd -- "$upstream"
    PSBC_JOBS="$jobs" bash toolchain/build-opengnm-psbc.sh
)
compiler="$upstream/third_party/opengnm-psbc/opengnm-psbc"
library="$upstream/third_party/opengnm-psbc/libpsbc.a"
[[ -x $compiler && -s $library ]] ||
    fail 'pinned host compiler executable or library was not built'

for stage in vert frag; do
    glslangValidator -V -S "$stage" \
        -o "$smoke/smoke.$stage.spv" "$repo_root/tools/shaders/smoke.$stage"
    spirv-val "$smoke/smoke.$stage.spv"
    source_stage=vertex
    if [[ $stage == frag ]]; then
        source_stage=fragment
    fi
    "$compiler" -g -s "$source_stage" \
        -f "$smoke/smoke.$stage.spv" \
        -o "$smoke/smoke.$stage.gfx1013.bin" \
        --raw --metadata "$smoke/smoke.$stage.metadata.json"
done

python3 "$repo_root/tools/verify_pinned_psbc.py" package \
    "$repo_root" "$upstream" "$smoke" "$artifact_dir"
[[ -s $artifact_dir/manifest.json ]] ||
    fail 'no verified compiler manifest was produced'
printf 'OpenAGC PSBC build: verified host compiler artifact ready\n'
