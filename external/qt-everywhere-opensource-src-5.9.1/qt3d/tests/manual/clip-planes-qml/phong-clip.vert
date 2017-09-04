#version 150 core

in vec3 vertexPosition;
in vec3 vertexNormal;

out VertexData {
    vec3 position;
    vec3 normal;
} v_out;

out float gl_ClipDistance[8];

struct Section {
    vec4 equation;
    vec3 center;
};

struct SectionsData {
    int sectionsCount;
    Section sections[8];
};

uniform SectionsData sectionsData;
uniform mat4 modelView;
uniform mat4 modelMatrix;
uniform mat3 modelViewNormal;
uniform mat4 mvp;

void main()
{
    v_out.normal = normalize( modelViewNormal * vertexNormal );
    v_out.position = vec4(modelView * vec4( vertexPosition, 1.0 )).xyz;
    vec3 worldPos = vec4(modelMatrix * vec4(vertexPosition, 1.0)).xyz;

    for (int i = 0; i < sectionsData.sectionsCount; ++i) {
        gl_ClipDistance[i] = dot(vec4(worldPos, 1.0), sectionsData.sections[i].equation);
    }

    gl_Position = mvp * vec4( vertexPosition, 1.0 );
}
