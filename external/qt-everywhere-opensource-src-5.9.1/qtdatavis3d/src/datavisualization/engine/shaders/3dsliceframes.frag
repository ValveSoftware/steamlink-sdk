#version 120

uniform highp vec4 color_mdl;
uniform highp vec2 sliceFrameWidth;

varying highp vec3 pos;

void main() {
    highp vec2 absPos = min(vec2(1.0, 1.0), abs(pos.xy));
    if (absPos.x > sliceFrameWidth.x || absPos.y > sliceFrameWidth.y)
        gl_FragColor = color_mdl;
    else
        discard;
}

