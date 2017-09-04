uniform sampler2D source;
uniform highp vec2 delta;
uniform highp float qt_Opacity;

varying highp vec2 qt_TexCoord0;

void main() {
    lowp vec4 tl = texture2D(source, qt_TexCoord0 - delta);
    lowp vec4 tr = texture2D(source, qt_TexCoord0 + vec2(delta.x, -delta.y));
    lowp vec4 bl = texture2D(source, qt_TexCoord0 - vec2(delta.x, -delta.y));
    lowp vec4 br = texture2D(source, qt_TexCoord0 + delta);
    mediump vec4 gx = (tl + bl) - (tr + br);
    mediump vec4 gy = (tl + tr) - (bl + br);
    gl_FragColor.xyz = vec3(0.);
    gl_FragColor.w = clamp(dot(sqrt(gx * gx + gy * gy), vec4(1.)), 0., 1.) * qt_Opacity;
}
