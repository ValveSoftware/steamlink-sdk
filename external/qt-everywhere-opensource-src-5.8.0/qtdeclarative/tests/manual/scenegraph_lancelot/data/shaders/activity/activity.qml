import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        id: text
        anchors.centerIn: parent
        font.pixelSize:  80
        text: "Shaderz!"
    }

    ShaderEffectSource {
        id: source
        sourceItem: text
        hideSource: effect.visible
    }

    ShaderEffect {
        id: effect
        anchors.fill: text

        property variant source: source

        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = vec4(qt_TexCoord0.x, qt_TexCoord0.y, 1, 1) * texture2D(source, qt_TexCoord0).a;
        }
        "
    }
}
