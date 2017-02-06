varying highp vec2 sampleCoord;
varying highp vec3 sampleFarLeft;
varying highp vec3 sampleNearLeft;
varying highp vec3 sampleNearRight;
varying highp vec3 sampleFarRight;

uniform sampler2D _qt_texture;
uniform lowp vec4 color;
uniform mediump float alphaMin;
uniform mediump float alphaMax;

void main()
{
    highp vec4 n;
    n.x = texture2DProj(_qt_texture, sampleFarLeft).a;
    n.y = texture2DProj(_qt_texture, sampleNearLeft).a;
    highp float c = texture2D(_qt_texture, sampleCoord).a;
    n.z = texture2DProj(_qt_texture, sampleNearRight).a;
    n.w = texture2DProj(_qt_texture, sampleFarRight).a;
#if 0
    // Blurrier, faster.
    n = smoothstep(alphaMin, alphaMax, n);
    c = smoothstep(alphaMin, alphaMax, c);
#else
    // Sharper, slower.
    highp vec2 d = min(abs(n.yw - n.xz) * 2., 0.67);
    highp vec2 lo = mix(vec2(alphaMin), vec2(0.5), d);
    highp vec2 hi = mix(vec2(alphaMax), vec2(0.5), d);
    n = smoothstep(lo.xxyy, hi.xxyy, n);
    c = smoothstep(lo.x + lo.y, hi.x + hi.y, 2. * c);
#endif
    gl_FragColor = vec4(0.333 * (n.xyz + n.yzw + c), c) * color.w;
}

/*
#extension GL_OES_standard_derivatives: enable

varying highp vec2 sampleCoord;

uniform sampler2D _qt_texture;
uniform lowp vec4 color;
uniform highp float alphaMin;
uniform highp float alphaMax;

void main()
{
    highp vec2 delta = dFdx(sampleCoord);
    highp vec4 n;
    n.x = texture2D(_qt_texture, sampleCoord - 0.667 * delta).a;
    n.y = texture2D(_qt_texture, sampleCoord - 0.333 * delta).a;
    highp float c = texture2D(_qt_texture, sampleCoord).a;
    n.z = texture2D(_qt_texture, sampleCoord + 0.333 * delta).a;
    n.w = texture2D(_qt_texture, sampleCoord + 0.667 * delta).a;
    n = smoothstep(alphaMin, alphaMax, n);
    c = smoothstep(alphaMin, alphaMax, c);
    gl_FragColor = vec4(0.333 * (n.xyz + n.yzw + c), c) * color.w;
};
*/