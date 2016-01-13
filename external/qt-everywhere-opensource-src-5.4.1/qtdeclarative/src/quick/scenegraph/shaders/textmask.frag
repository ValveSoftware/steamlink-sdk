varying highp vec2 sampleCoord;

uniform sampler2D _qt_texture;
uniform lowp vec4 color;

void main()
{
    lowp vec4 glyph = texture2D(_qt_texture, sampleCoord);
    gl_FragColor = vec4(glyph.rgb * color.a, glyph.a);
}