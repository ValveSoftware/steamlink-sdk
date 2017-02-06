attribute highp vec4 v;
uniform highp mat4 matrix;
uniform highp mat4 rotation;

// set to 1 if projection is enabled
uniform bool projection;

varying mediump vec2 pos;

void main()
{
    vec4 p = matrix * v;

    if (projection) {
        vec4 proj = rotation * p;
        gl_Position = vec4(proj.x, proj.y, 0, proj.z);
    } else {
        gl_Position = p;
    }

    pos = v.xy * 1.37;
}
