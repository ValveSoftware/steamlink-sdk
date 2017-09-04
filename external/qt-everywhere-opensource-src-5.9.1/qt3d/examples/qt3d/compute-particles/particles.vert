#version 430 core

in vec3 vertexPosition;
in vec3 vertexNormal;

in vec3 particlePosition;
in vec3 particleColor;

out VertexBlock
{
    flat vec3 color;
    vec3 pos;
    vec3 normal;
} v_out;

uniform mat4 mvp;
uniform mat3 modelViewNormal;
uniform mat4 modelView;

void main(void)
{
    vec4 pos = vec4(vertexPosition.xyz, 1.0) + vec4(particlePosition, 0.0);
    gl_Position = mvp * pos;
    v_out.pos = vec4(modelView * pos).xyz;
    v_out.normal = normalize(modelViewNormal * vertexNormal);
    v_out.color = mix(particleColor * 0.2, particleColor, smoothstep(0.5, 0.8, abs(v_out.normal).z));
}
