varying highp vec2 sampleCoord;

uniform mediump sampler2D _qt_texture;
uniform lowp vec4 color;
uniform mediump float alphaMin;
uniform mediump float alphaMax;

void main()
{
    gl_FragColor = color * smoothstep(alphaMin,
                                      alphaMax,
                                      texture2D(_qt_texture, sampleCoord).a);
}