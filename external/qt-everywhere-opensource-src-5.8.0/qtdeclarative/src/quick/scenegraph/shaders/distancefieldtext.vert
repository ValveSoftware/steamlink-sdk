uniform highp mat4 matrix;
uniform highp vec2 textureScale;

attribute highp vec4 vCoord;
attribute highp vec2 tCoord;

varying highp vec2 sampleCoord;

void main()
{
     sampleCoord = tCoord * textureScale;
     gl_Position = matrix * vCoord;
}