varying highp vec3 pos;

void main() {
    // This shader encodes the axis position into the vertex color, assuming the object
    // is a cube filling the entire data area of the graph
    gl_FragColor = vec4((pos.x + 1.0) / 2.0,
                        (pos.y + 1.0) / 2.0,
                        (-pos.z + 1.0) / 2.0,
                        0.0);
}

