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
            transform: Rotation{ angle: 45}
        }
    }

    ShaderEffectSource {
        id: source1
        sourceItem: text
        smooth: true
    }

    ShaderEffectSource {
        id: source2
        sourceItem: text
        smooth: true
    }

    ShaderEffect{
        width: parent.width
        height: parent.height / 2

        property variant source: source1

        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = vec4(1, qt_TexCoord0.y, 0, 1) * texture2D(source, qt_TexCoord0).a;
        }
        "
    }

    ShaderEffect {
        y: parent.height / 2
        width: parent.width
        height: parent.height / 2

        property variant source: source2

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

