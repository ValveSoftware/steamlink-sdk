varying highp vec2 sampleCoord;

uniform lowp sampler2D _qt_texture;
uniform lowp float color; // just the alpha, really...

void main()
{
    lowp vec4 glyph = texture2D(_qt_texture, sampleCoord);
    gl_FragColor = vec4(glyph.rgb * color, glyph.a);
}