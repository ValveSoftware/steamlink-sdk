varying highp vec3 sampleNearLeft;
varying highp vec3 sampleNearRight;

uniform sampler2D _qt_texture;
uniform lowp vec4 color;
uniform mediump float alphaMin;
uniform mediump float alphaMax;

void main()
{
    highp vec2 n;
    n.x = texture2DProj(_qt_texture, sampleNearLeft).a;
    n.y = texture2DProj(_qt_texture, sampleNearRight).a;
    n = smoothstep(alphaMin, alphaMax, n);
    highp float c = 0.5 * (n.x + n.y);
    gl_FragColor = vec4(n.x, c, n.y, c) * color.w;
}