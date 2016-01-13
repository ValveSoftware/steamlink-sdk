#version 150 core

in vec2 vPos;
in vec2 vTex;

out vec4 fTexS;
out float progress;

uniform vec3 animData;  // w,h(premultiplied of anim), interpolation progress
uniform vec4 animPos;   // x,y, x,y (two frames for interpolation)
uniform mat4 qt_Matrix;

void main()
{
    progress = animData.z;

    // Calculate frame location in texture
    fTexS.xy = animPos.xy + vTex.xy * animData.xy;

    // Next frame is also passed, for interpolation
    fTexS.zw = animPos.zw + vTex.xy * animData.xy;

    gl_Position = qt_Matrix * vec4(vPos.x, vPos.y, 0, 1);
}