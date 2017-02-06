#version 150 core

in vec4 fTexS;
in float progress;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform float qt_Opacity;

void main()
{
    fragColor = mix(texture(_qt_texture, fTexS.xy),
                    texture(_qt_texture, fTexS.zw),
                    progress) * qt_Opacity;
}