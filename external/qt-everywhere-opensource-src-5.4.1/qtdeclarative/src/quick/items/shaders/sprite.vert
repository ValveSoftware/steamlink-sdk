attribute highp vec2 vPos;
attribute highp vec2 vTex;

uniform highp vec3 animData;// w,h(premultiplied of anim), interpolation progress
uniform highp vec4 animPos;//x,y, x,y (two frames for interpolation)

uniform highp mat4 qt_Matrix;

varying highp vec4 fTexS;
varying lowp float progress;

void main()
{
    progress = animData.z;

    // Calculate frame location in texture
    fTexS.xy = animPos.xy + vTex.xy * animData.xy;

    // Next frame is also passed, for interpolation
    fTexS.zw = animPos.zw + vTex.xy * animData.xy;

    gl_Position = qt_Matrix * vec4(vPos.x, vPos.y, 0, 1);
}