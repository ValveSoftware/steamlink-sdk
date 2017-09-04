#version 150 core

uniform sampler2D tex;

in vec2 t;

out vec4 fragColor;

void main()
{
    fragColor = texture(tex, t);
}
