uniform sampler2D plane1Texture;
uniform sampler2D plane2Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;
varying highp vec2 plane1TexCoord;
varying highp vec2 plane2TexCoord;

void main()
{
    mediump float Y = texture2D(plane1Texture, plane1TexCoord).r;
    mediump vec2 UV = texture2D(plane2Texture, plane2TexCoord).ar;
    mediump vec4 color = vec4(Y, UV.x, UV.y, 1.);
    gl_FragColor = colorMatrix * color * opacity;
}
