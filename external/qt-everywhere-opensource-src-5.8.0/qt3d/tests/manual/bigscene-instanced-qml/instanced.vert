#version 150 core

#define M_PI 3.14159

in vec3 vertexPosition;
in vec3 vertexNormal;

out vec3 position;
out vec3 normal;
out vec3 kd;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 modelViewNormal;
uniform int instanceCount;

uniform float time;

const vec3 ZAXIS = vec3(0.0, 0.0, 1.0);
const vec3 XAXIS = vec3(1.0, 0.0, 0.0);
const float radius = 30.0;
float det = 1.0 / float(instanceCount);

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}


mat4 translationMatrix(vec3 translation)
{
    return mat4(1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                translation.x, translation.y, translation.z, 0);

}

void main()
{
    float angle = M_PI * 2.0f * gl_InstanceID * det * 10.0 * time * 0.05;
    vec3 translation = vec3(radius * cos(angle), 200.0 * gl_InstanceID * det, radius * sin(angle));

    mat4 modelMatrix = translationMatrix(translation) *
                       rotationMatrix(ZAXIS, radians(angle * 45.0)) *
                       rotationMatrix(XAXIS, radians(angle * 30.0));

    vec4 eyePosition = viewMatrix *
                       modelMatrix *
                       vec4(vertexPosition, 1.0);

    position = eyePosition.xyz;
    kd = vec3(abs(cos(angle)), 0.75, 25);
    normal = normalize(mat3(modelMatrix) * modelViewNormal *
                       vertexNormal);

    gl_Position = projectionMatrix * vec4(position, 1.0);
}
