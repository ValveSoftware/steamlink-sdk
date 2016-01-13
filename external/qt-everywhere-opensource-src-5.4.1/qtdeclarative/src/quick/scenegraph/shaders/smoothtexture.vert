uniform highp vec2 pixelSize;
uniform highp mat4 qt_Matrix;
uniform lowp float opacity;

attribute highp vec4 vertex;
attribute highp vec2 multiTexCoord;
attribute highp vec2 vertexOffset;
attribute highp vec2 texCoordOffset;

varying highp vec2 texCoord;
varying lowp float vertexOpacity;

void main()
{
    highp vec4 pos = qt_Matrix * vertex;
    gl_Position = pos;
    texCoord = multiTexCoord;

    if (vertexOffset.x != 0.) {
        highp vec4 delta = qt_Matrix[0] * vertexOffset.x;
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
        texCoord.x += scale * texCoordOffset.x;
    }

    if (vertexOffset.y != 0.) {
        highp vec4 delta = qt_Matrix[1] * vertexOffset.y;
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
        texCoord.y += scale * texCoordOffset.y;
    }

    bool onEdge = any(notEqual(vertexOffset, vec2(0.)));
    bool outerEdge = all(equal(texCoordOffset, vec2(0.)));
    vertexOpacity = onEdge && outerEdge ? 0. : opacity;
}