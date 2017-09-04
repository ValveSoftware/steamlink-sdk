#version 150

uniform vec2 winsize;
uniform float time;

out vec4 fragColor;

float displacement(vec3 p)
{
    float cosT = cos(time);
    float sinT = sin(time);

    mat2 mat = mat2(cosT, -sinT, sinT, cosT);
    p.xz *= mat;
    p.xy *= mat;
    vec3 q = 1.75 * p;

    return length(p + vec3(sinT)) *
           log(length(p) + 1.0) +
            sin(q.x + sin(q.z + sin(q.y))) * 0.25 - 1.0;
}

void main()
{
    vec3 color;
    float d = 2.5;
    vec2 screenPos = gl_FragCoord.xy / winsize.xy - vec2(0.6, 0.4);
    vec3 pos = normalize(vec3(screenPos, -1.0));
    float sinT = sin(time) * 0.2;

    // compute plasma color
    for (int i = 0; i < 8; ++i) {
        vec3 p = vec3(0.0, 0.0, 5.0) + pos * d;

        float positionFactor = displacement(p);
        d += min(positionFactor, 1.0);

        float clampFactor =  clamp((positionFactor- displacement(p + 0.1)) * 0.5, -0.1, 1.0);
        vec3 l = vec3(0.2 * sinT, 0.35, 0.4) + vec3(5.0, 2.5, 3.25) * clampFactor;
        color = (color + (1.0 - smoothstep(0.0, 2.5, positionFactor)) * 0.7) * l;
    }

    // background color + plasma color
    fragColor = vec4(screenPos * (vec2(1.0, 0.5) + sinT), 0.5 + sinT, 1.0) + vec4(color, 1.0);
}
