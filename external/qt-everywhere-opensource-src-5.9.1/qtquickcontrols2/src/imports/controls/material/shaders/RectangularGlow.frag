uniform highp float qt_Opacity;
uniform mediump float relativeSizeX;
uniform mediump float relativeSizeY;
uniform mediump float spread;
uniform lowp vec4 color;
varying highp vec2 qt_TexCoord0;

highp float linearstep(highp float e0, highp float e1, highp float x) {
    return clamp((x - e0) / (e1 - e0), 0.0, 1.0);
}

void main() {
    lowp float alpha =
        smoothstep(0.0, relativeSizeX, 0.5 - abs(0.5 - qt_TexCoord0.x)) *
        smoothstep(0.0, relativeSizeY, 0.5 - abs(0.5 - qt_TexCoord0.y));

    highp float spreadMultiplier = linearstep(spread, 1.0 - spread, alpha);
    gl_FragColor = color * qt_Opacity * spreadMultiplier * spreadMultiplier;
}
