uniform sampler2D textureSampler;

varying highp vec2 UV;

void main() {
    gl_FragColor = texture2D(textureSampler, UV);
}

