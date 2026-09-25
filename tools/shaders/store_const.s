/* OpenAGC original gfx1013 kernel: store imm 0xA5A5A5A5 to VA in s2:s3.
 * No loads. Assemble with:
 *   llvm-mc -triple=amdgcn -mcpu=gfx1013 -filetype=obj store_const.s -o store_const.o
 *   llvm-objcopy --dump-section=.text=store_const.bin store_const.o
 */
.text
.globl openagc_store_const
openagc_store_const:
    v_mov_b32 v2, s2
    v_mov_b32 v3, s3
    v_mov_b32 v0, 0xA5A5A5A5
    flat_store_dword v[2:3], v0
    s_endpgm
