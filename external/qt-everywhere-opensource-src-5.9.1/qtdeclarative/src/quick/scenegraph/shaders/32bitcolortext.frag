varying highp vec2 sampleCoord;

uniform sampler2D _qt_texture;
uniform lowp float color; // just the alpha, really...

void main()
{
    gl_FragColor = texture2D(_qt_texture, sampleCoord) * color;
}
