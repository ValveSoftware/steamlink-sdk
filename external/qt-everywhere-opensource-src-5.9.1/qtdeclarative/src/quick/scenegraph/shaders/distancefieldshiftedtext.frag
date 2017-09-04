varying highp vec2 sampleCoord;
varying highp vec2 shiftedSampleCoord;

uniform mediump sampler2D _qt_texture;
uniform lowp vec4 color;
uniform lowp vec4 styleColor;
uniform mediump float alphaMin;
uniform mediump float alphaMax;

void main()
{
    highp float a = smoothstep(alphaMin, alphaMax, texture2D(_qt_texture, sampleCoord).a);
    highp vec4 shifted = styleColor * smoothstep(alphaMin,
                                                 alphaMax,
                                                 texture2D(_qt_texture, shiftedSampleCoord).a);
    gl_FragColor = mix(shifted, color, a);
}