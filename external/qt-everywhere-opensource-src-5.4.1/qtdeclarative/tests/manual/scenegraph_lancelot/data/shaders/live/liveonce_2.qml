import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        id: text
        anchors.centerIn: parent
        font.pixelSize:  80
        text: "Shaderz!"
        visible: false

        Rectangle {
            width: 50
            height: 50
            color: "red"
            anchors.centerIn: parent
            transform: Rotation { angle: 90 }
        }
    }

    ShaderEffectSource {
        id: source
        sourceItem: text
        smooth: true
    }

    ShaderEffect {
        width: parent.width
        height: parent.height / 2

        property variant source: source

        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = vec4(0, qt_TexCoord0.y, 1, 1) * texture2D(source, qt_TexCoord0).a;
        }
        "
    }
}
