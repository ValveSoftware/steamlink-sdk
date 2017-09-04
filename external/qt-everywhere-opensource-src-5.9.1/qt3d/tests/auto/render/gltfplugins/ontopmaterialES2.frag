uniform lowp vec4 handleColor;
uniform lowp float customAlpha;

uniform sampler2D customTexture;

void main()
{
    vec3 customColor = texture(customTexture, vec2(0.4, 0.4)).rgb;
    gl_FragColor = vec4(handleColor.xy, customColor.z, customAlpha);
}
