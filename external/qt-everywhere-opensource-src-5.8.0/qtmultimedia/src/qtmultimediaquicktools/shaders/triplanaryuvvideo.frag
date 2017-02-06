uniform sampler2D plane1Texture;
uniform sampler2D plane2Texture;
uniform sampler2D plane3Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

varying highp vec2 plane1TexCoord;
varying highp vec2 plane2TexCoord;
varying highp vec2 plane3TexCoord;

void main()
{
    mediump float Y = texture2D(plane1Texture, plane1TexCoord).r;
    mediump float U = texture2D(plane2Texture, plane2TexCoord).r;
    mediump float V = texture2D(plane3Texture, plane3TexCoord).r;
    mediump vec4 color = vec4(Y, U, V, 1.);
    gl_FragColor = colorMatrix * color * opacity;
}
