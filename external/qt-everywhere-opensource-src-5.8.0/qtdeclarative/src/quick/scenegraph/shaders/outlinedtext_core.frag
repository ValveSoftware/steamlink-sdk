#version 150 core

in vec2 sampleCoord;
in vec2 sCoordUp;
in vec2 sCoordDown;
in vec2 sCoordLeft;
in vec2 sCoordRight;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform vec4 color;
uniform vec4 styleColor;

void main()
{
    float glyph = texture(_qt_texture, sampleCoord).r;
    float outline = clamp(clamp(texture(_qt_texture, sCoordUp).r +
                                texture(_qt_texture, sCoordDown).r +
                                texture(_qt_texture, sCoordLeft).r +
                                texture(_qt_texture, sCoordRight).r,
                                0.0, 1.0) - glyph,
                          0.0, 1.0);
    fragColor = outline * styleColor + glyph * color;
}