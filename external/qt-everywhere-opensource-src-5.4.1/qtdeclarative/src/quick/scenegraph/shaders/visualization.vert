attribute highp vec4 v;
uniform highp mat4 matrix;
uniform highp mat4 rotation;

// w -> apply 3d rotation and projection
uniform mediump vec4 tweak;

varying mediump vec2 pos;

void main()
{
    vec4 p = matrix * v;

    if (tweak.w > 0.0) {
        vec4 proj = rotation * p;
        gl_Position = vec4(proj.x, proj.y, 0, proj.z);
    } else {
        gl_Position = p;
    }

    pos = v.xy * 1.37;
}
