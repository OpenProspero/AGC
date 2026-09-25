/* OpenAGC original gfx1013 kernel: 8 lanes store imm 0xA5A5A5A5 to
 * VA s2:s3 + tid*4 via flat_store_dword (vaddr pair v[2:3]).
 * PAI FW9.40: v0=tid, flat_store pair 1 works, lanes 0..7 write.
 * High VA half is s3 unchanged (offset < 4 GiB from base).
 * No loads. Assemble with:
 *   llvm-mc -triple=amdgcn -mcpu=gfx1013 -filetype=obj store_span.s -o store_span.o
 *   llvm-objcopy --dump-section=.text=store_span.bin store_span.o
 */
.text
.globl openagc_store_span
openagc_store_span:
    v_lshlrev_b32_e32 v1, 2, v0
    v_add_nc_u32_e32 v2, s2, v1
    v_mov_b32_e32 v3, s3
    v_mov_b32_e32 v0, 0xA5A5A5A5
    flat_store_dword v[2:3], v0
    s_endpgm
