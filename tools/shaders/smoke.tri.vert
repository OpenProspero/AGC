#version 450

/* Smoke triangle: one big triangle that covers the whole viewport rect,
 * placed from gl_VertexIndex so the NGG vertex stage's vertex-index path is
 * exercised by the same draw. The pixel shader is smoke.frag, so the
 * acceptance value is unchanged. */

void main()
{
    vec2 p = vec2(-1.0, -1.0) + vec2(gl_VertexIndex == 1 ? 4.0 : 0.0,
                                     gl_VertexIndex == 2 ? 4.0 : 0.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
