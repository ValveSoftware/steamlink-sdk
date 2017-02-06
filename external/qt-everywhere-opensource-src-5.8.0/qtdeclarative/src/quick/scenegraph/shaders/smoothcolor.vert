uniform highp vec2 pixelSize;
uniform highp mat4 matrix;
uniform lowp float opacity;

attribute highp vec4 vertex;
attribute lowp vec4 vertexColor;
attribute highp vec2 vertexOffset;

varying lowp vec4 color;

void main()
{
    highp vec4 pos = matrix * vertex;
    gl_Position = pos;

    if (vertexOffset.x != 0.) {
        highp vec4 delta = matrix[0] * vertexOffset.x;
        highp vec2 dir = delta.xy * pos.w - pos.xy * delta.w;
        highp vec2 ndir = .5 * pixelSize * normalize(dir / pixelSize);
        dir -= ndir * delta.w * pos.w;
        highp float numerator = dot(dir, ndir * pos.w * pos.w);
        highp float scale = 0.0;
        if (numerator < 0.0)
            scale = 1.0;
        else
            scale = min(1.0, numerator / dot(dir, dir));
        gl_Position += scale * delta;
    }

    if (vertexOffset.y != 0.) {
        highp vec4 delta = matrix[1] * vertexOffset.y;
        highp vec2 dir = delta.xy * pos.w - pos.xy * delta.w;
        highp vec2 ndir = .5 * pixelSize * normalize(dir / pixelSize);
        dir -= ndir * delta.w * pos.w;
        highp float numerator = dot(dir, ndir * pos.w * pos.w);
        highp float scale = 0.0;
        if (numerator < 0.0)
            scale = 1.0;
        else
            scale = min(1.0, numerator / dot(dir, dir));
        gl_Position += scale * delta;
    }

    color = vertexColor * opacity;
}