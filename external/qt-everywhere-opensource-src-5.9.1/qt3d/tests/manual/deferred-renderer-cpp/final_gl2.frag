#version 110

uniform sampler2D color;
uniform sampler2D position;
uniform sampler2D normal;
uniform vec2 winSize;

const int MAX_LIGHTS = 8;
const int TYPE_POINT = 0;
const int TYPE_DIRECTIONAL = 1;
const int TYPE_SPOT = 2;

struct Light {
    int type;
    vec3 position;
    vec3 color;
    float intensity;
    vec3 direction;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
    float cutOffAngle;
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

void main()
{
    vec2 texCoord = gl_FragCoord.xy / winSize;
    vec4 col = texture2D(color, texCoord);
    vec3 pos = texture2D(position, texCoord).xyz;
    vec3 norm = texture2D(normal, texCoord).xyz;

    vec3 lightColor;
    vec3 s;

    for (int i = 0; i < lightCount; ++i) {
        float att = 1.0;
        if ( lights[i].type != TYPE_DIRECTIONAL ) {
            s = lights[i].position - pos;
            if (lights[i].constantAttenuation != 0.0
             || lights[i].linearAttenuation != 0.0
             || lights[i].quadraticAttenuation != 0.0) {
                float dist = length(s);
                att = 1.0 / (lights[i].constantAttenuation + lights[i].linearAttenuation * dist + lights[i].quadraticAttenuation * dist * dist);
            }
            s = normalize( s );
            if ( lights[i].type == TYPE_SPOT ) {
                if ( degrees(acos(dot(-s, normalize(lights[i].direction))) ) > lights[i].cutOffAngle)
                    att = 0.0;
            }
        } else {
            s = normalize( -lights[i].direction );
        }

        float diffuse = max( dot( s, norm ), 0.0 );

        lightColor += att * lights[i].intensity * diffuse * lights[i].color;
    }
    gl_FragColor = vec4(col.rgb * lightColor, col.a);
}
