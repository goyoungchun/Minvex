@ctype mat4 lv::Mat4
@block particle_draw_data
struct particle_draw_item {
    vec4 center_size;
    uvec4 style;
};
@end
@cs particle_compute_cs
@include_block particle_draw_data
struct particle_input_item {
    vec4 origin_age;
    vec4 velocity_drag;
    vec4 acceleration_size;
    uvec4 style;
};
layout(binding=0) readonly buffer particle_input { particle_input_item input_items[]; };
layout(binding=1) buffer particle_output { particle_draw_item output_items[]; };
layout(binding=0) uniform particle_compute_params { mat4 view; ivec4 counts; };
layout(local_size_x=64, local_size_y=1, local_size_z=1) in;
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(counts.x)) { return; }
    particle_input_item p = input_items[i];
    float t = p.origin_age.w, drag = p.velocity_drag.w, x = drag * t;
    float velocity = t, acceleration = 0.5 * t * t;
    if (x < 0.02) {
        velocity *= 1.0 - x * (0.5 - x * (1.0 / 6.0 - x / 24.0));
        acceleration = t * t * (0.5 - x / 6.0 + x * x / 24.0);
    } else {
        velocity = (1.0 - exp(-x)) / drag;
        acceleration = (t - velocity) / drag;
    }
    vec3 world = p.origin_age.xyz + p.velocity_drag.xyz * velocity + p.acceleration_size.xyz * acceleration;
    vec3 center = (view * vec4(world, 1.0)).xyz;
    float distance_limit = uintBitsToFloat(p.style.z);
    float size = p.acceleration_size.w;
    if (distance_limit > 0.0 && dot(center, center) > distance_limit * distance_limit) { size = 0.0; }
    output_items[i].center_size = vec4(center, size);
    output_items[i].style = p.style;
}
@end
@program particle_compute particle_compute_cs

@vs particle_compute_draw_vs
@include_block particle_draw_data
layout(binding=0) readonly buffer particle_draw_buffer { particle_draw_item draw_items[]; };
layout(binding=0) uniform particle_compute_draw_params { mat4 projection; };
out vec2 uv;
out vec4 tint;
void main() {
    particle_draw_item p = draw_items[gl_InstanceIndex];
    vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1)) * 2.0 - 1.0;
    float rotation = uintBitsToFloat(p.style.x), aspect = uintBitsToFloat(p.style.y);
    float c = cos(rotation), s = sin(rotation);
    vec2 offset = mat2(c,s,-s,c) * (corner * vec2(1.0, aspect) * p.center_size.w * 0.5);
    gl_Position = projection * vec4(p.center_size.xyz + vec3(offset, 0.0), 1.0);
    uv = corner;
    uint rgba = p.style.w;
    tint = vec4(float(rgba & 255u), float((rgba >> 8u) & 255u),
                float((rgba >> 16u) & 255u), float(rgba >> 24u)) / 255.0;
}
@end
@fs particle_compute_draw_fs
in vec2 uv;
in vec4 tint;
out vec4 frag_color;
void main() {
    frag_color = vec4(tint.rgb, tint.a * (1.0 - smoothstep(0.15, 1.0, dot(uv, uv))));
}
@end
@program particle_compute_draw particle_compute_draw_vs particle_compute_draw_fs
