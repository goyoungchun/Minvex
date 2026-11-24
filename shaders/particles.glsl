@ctype mat4 lv::Mat4
@vs particle_vs
layout(binding=0) uniform particle_params { mat4 projection; mat4 view; vec4 atlas; vec4 kind; };
in vec3 corner;
in vec4 center_size;
in vec2 rotation_aspect;
in vec4 color0;
in float frame_index;
out vec2 uv;
out vec4 color;
out vec3 view_position;
flat out float textured;
flat out float geometry;
void main() {
    float s = sin(rotation_aspect.x), c = cos(rotation_aspect.x);
    vec2 local = corner.xy * vec2(1.0, rotation_aspect.y) * (center_size.w * 0.5);
    vec2 offset = mat2(c, s, -s, c) * local;
    geometry = kind.x;
    vec3 displacement = vec3(offset, geometry >= 2.0 ? corner.y * frame_index * 0.5 : 0.0);
    if (geometry == 1.0) {
        vec3 rotated = vec3(c * corner.x + s * corner.z, corner.y, c * corner.z - s * corner.x);
        displacement = (view * vec4(rotated * center_size.w, 0.0)).xyz;
    }
    view_position = center_size.xyz + displacement;
    gl_Position = projection * vec4(view_position, 1.0);
    textured = float(atlas.z > 0.0);
    float frame = geometry == 0.0 ? frame_index : 0.0;
    vec2 tile = vec2(mod(frame, atlas.x), floor(frame / atlas.x));
    vec2 corner01 = vec2(corner.x, -corner.y) * 0.5 + 0.5;
    vec2 tileSize = 1.0 / atlas.xy;
    uv = textured > 0.5 ? tile * tileSize + mix(atlas.zw, tileSize - atlas.zw, corner01) : corner.xy;
    color = color0;
}
@end
@fs particle_fs
in vec2 uv;
in vec4 color;
in vec3 view_position;
flat in float textured;
flat in float geometry;
layout(binding=0) uniform texture2D particle_texture;
layout(binding=0) uniform sampler particle_sampler;
out vec4 frag_color;
void main() {
    if (textured > 0.5) {
        frag_color = color * texture(sampler2D(particle_texture, particle_sampler), uv);
    } else if (geometry == 1.0) {
        vec3 n = cross(dFdx(view_position), dFdy(view_position));
        float diffuse = abs(dot(n, vec3(0.3, 0.5, 0.81))) * inversesqrt(max(dot(n,n), 1e-20));
        frag_color = vec4(color.rgb * (0.45 + 0.55 * diffuse), color.a);
    } else if (geometry >= 2.0) {
        frag_color = vec4(color.rgb, color.a * (1.0 - smoothstep(0.75, 1.0, abs(uv.x))));
    } else {
        float coverage = 1.0 - smoothstep(0.15, 1.0, dot(uv, uv));
        frag_color = vec4(color.rgb, color.a * coverage);
    }
}
@end
@program particle_sprite particle_vs particle_fs
