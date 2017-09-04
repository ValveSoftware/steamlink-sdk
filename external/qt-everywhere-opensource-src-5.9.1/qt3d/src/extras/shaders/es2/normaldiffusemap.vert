attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
attribute vec2 vertexTexCoord;
attribute vec4 vertexTangent;

varying vec3 worldPosition;
varying vec2 texCoord;
varying mat3 tangentMatrix;

uniform mat4 modelMatrix;
uniform mat3 modelNormalMatrix;
uniform mat4 projectionMatrix;
uniform mat4 mvp;

uniform float texCoordScale;

void main()
{
    // Pass through texture coordinates
    texCoord = vertexTexCoord * texCoordScale;

    // Transform position, normal, and tangent to world coords
    vec3 normal = normalize( modelNormalMatrix * vertexNormal );
    vec3 tangent = normalize( modelNormalMatrix * vertexTangent.xyz );
    worldPosition = vec3( modelMatrix * vec4( vertexPosition, 1.0 ) );

    // Calculate binormal vector
    vec3 binormal = normalize( cross( normal, tangent ) );

    // Construct matrix to transform from eye coords to tangent space
    tangentMatrix = mat3 (
                tangent.x, binormal.x, normal.x,
                tangent.y, binormal.y, normal.y,
                tangent.z, binormal.z, normal.z );

    // Calculate vertex position in clip coordinates
    gl_Position = mvp * vec4( vertexPosition, 1.0 );
}
