#version 150 core

out vec4 fragColor;

in FragData {
    vec3 fragPosition;
    vec3 fragNormal;
} f_in;

void main()
{
    fragColor = vec4(0.0, 1.0, 0.0, 1.0) + (1.0 * vec4(f_in.fragPosition, 1.0));
}
