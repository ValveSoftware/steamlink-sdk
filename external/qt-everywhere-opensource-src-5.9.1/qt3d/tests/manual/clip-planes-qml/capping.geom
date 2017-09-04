#version 150 core

layout( triangles ) in;
layout( triangle_strip, max_vertices = 24 ) out;

struct Section {
    vec4 equation;
    vec3 center;
};

struct SectionsData {
    int sectionsCount;
    Section sections[8];
};

in VertexData {
    vec3 position;
} v_in[];

out FragData {
    vec3 fragPosition;
    vec3 fragNormal;
} v_out;

uniform SectionsData sectionsData;
uniform mat4 mvp;

const float radius = 1000.0;

void emitQuad(Section section)
{
    vec4 equation = section.equation;
    vec3 center = section.center;
    v_out.fragNormal = equation.xyz;

    // generate tangent and bitangent
    vec3 u = vec3(1.0f, 1.0f, 1.0f);

    // 2 vectors a (xa, ya, za), b (xb, yb, zb) are orthogonal if:
    // xaxb + yayb + zazb = 0 <=>
    // zb = -(xaxb + yayb) / za
    // yb = -(xaxb + zazb) / ya
    // xb = -(yayb + zazb) / xa

    if (equation.x != 0)
        u.x = -(u.y * equation.y + u.z * equation.z) / equation.x;
    else if (equation.y != 0)
        u.y = -(u.x * equation.x + u.z * equation.z) / equation.y;
    else if (equation.z != 0)
        u.z = -(u.x* equation.x + u.y * equation.y) / equation.z;

    u = normalize(u);
    vec3 v = normalize(cross(u, equation.xyz)) * radius;
    u *= radius;

    v_out.fragPosition = v_in[0].position;
    gl_Position = mvp * vec4(center + u + v, 1.0);
    EmitVertex();

    v_out.fragPosition = v_in[1].position;
    gl_Position = mvp * vec4(center + u - v, 1.0);
    EmitVertex();

    v_out.fragPosition = v_in[2].position;
    gl_Position = mvp * vec4(center - u + v, 1.0);
    EmitVertex();

    v_out.fragPosition = v_in[0].position;
    gl_Position = mvp * vec4(center - u - v, 1.0);
    EmitVertex();

    EndPrimitive();
}

void main()
{
    for (int i = 0; i < sectionsData.sectionsCount; ++i) {
        emitQuad(sectionsData.sections[i]);
    }
}
