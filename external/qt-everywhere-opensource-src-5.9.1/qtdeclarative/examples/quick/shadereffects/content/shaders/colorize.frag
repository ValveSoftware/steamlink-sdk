uniform sampler2D source;
uniform lowp vec4 tint;
uniform lowp float qt_Opacity;

varying highp vec2 qt_TexCoord0;

void main() {
    lowp vec4 c = texture2D(source, qt_TexCoord0);
    lowp float lo = min(min(c.x, c.y), c.z);
    lowp float hi = max(max(c.x, c.y), c.z);
    gl_FragColor = qt_Opacity * vec4(mix(vec3(lo), vec3(hi), tint.xyz), c.w);
}
