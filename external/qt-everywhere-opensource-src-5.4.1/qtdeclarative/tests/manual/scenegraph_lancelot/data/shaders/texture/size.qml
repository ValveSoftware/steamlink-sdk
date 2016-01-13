import QtQuick 2.0

Item {
    width: 320
    height: 480

    ShaderEffectSource {
        id: source
        sourceItem: text
        textureSize: Qt.size(text.width / 2, text.height / 2)
        smooth: true
    }

    ShaderEffect {
        anchors.fill: text

        property variant source: source
        property variant textureSize: source.textureSize
        property color color: "black"

        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        uniform highp vec2 textureSize;
        uniform lowp vec4 color;
        uniform lowp float qt_Opacity;
        void main() {
            highp vec2 dx = vec2(0.5 / textureSize.x, 0.);
            highp vec2 dy = vec2(0., 0.5 / textureSize.y);
            gl_FragColor = color * 0.25
                         * (texture2D(source, qt_TexCoord0 + dx + dy).a
                         + texture2D(source, qt_TexCoord0 + dx - dy).a
                         + texture2D(source, qt_TexCoord0 - dx + dy).a
                         + texture2D(source, qt_TexCoord0 - dx - dy).a);
        }
        "
    }

    Text {
        id: text
        anchors.centerIn: parent
        font.pixelSize:  80
        text: "Shaderz!"
        color: "white"
    }
}
