#version 150

in vec3 normal;
in vec3 position;
in vec2 texCoord;

uniform vec3 finalColor;

out vec4 fragColor;

void main()
{
    vec3 n = normalize(normal);
    vec3 s = normalize(vec3(1.0, 0.0, 1.0) - position);
    vec3 v = normalize(-position);

    float diffuse = max(dot(s, n), 0.0);

    if (diffuse > 0.95)
        diffuse = 1.0;
    else if (diffuse > 0.5)
        diffuse = 0.5;
    else if (diffuse > 0.25)
        diffuse = 0.25;
    else
        diffuse = 0.1;

    fragColor = vec4(diffuse * finalColor, 1.0);
}
