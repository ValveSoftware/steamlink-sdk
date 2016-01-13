import QtQuick 2.0

Item {
    width: 320
    height: 480

    Rectangle {
        id: rect;
        anchors.centerIn: parent
        width: 1
        height: 10
        visible: false

        gradient: Gradient {
            GradientStop { position: 0; color: "#ff0000" }
            GradientStop { position: 0.5; color: "#00ff00" }
            GradientStop { position: 1; color: "#0000ff" }
        }
    }

    Text {
        id: text
        anchors.centerIn: parent
        font.pixelSize:  80
        text: "Shaderz!"
        visible: false
    }

    ShaderEffectSource {
        id: maskSource
        sourceItem: text
        smooth: true
    }

    ShaderEffectSource {
        id: colorSource
        sourceItem: rect;
        smooth: true
    }

    ShaderEffect {
        anchors.fill: text;

        property variant colorSource: colorSource
        property variant maskSource: maskSource;

        fragmentShader: "
        uniform lowp sampler2D maskSource;
        uniform lowp sampler2D colorSource;
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        void main() {
            gl_FragColor = texture2D(maskSource, qt_TexCoord0).a * texture2D(colorSource, qt_TexCoord0.yx);
        }
        "
    }
}
