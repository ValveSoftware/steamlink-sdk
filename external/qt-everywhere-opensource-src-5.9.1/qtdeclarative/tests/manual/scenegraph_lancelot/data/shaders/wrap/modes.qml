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
    }

    ShaderEffectSource {
        id: clamp
        sourceItem: text
        smooth: true
        wrapMode: ShaderEffectSource.ClampToEdge
    }

    ShaderEffectSource {
        id: hRepeat
        sourceItem: text
        smooth: true
        wrapMode: ShaderEffectSource.RepeatHorizontally
    }

    ShaderEffectSource {
        id: vRepeat
        sourceItem: text
        smooth: true
        wrapMode: ShaderEffectSource.RepeatVertically
    }

    ShaderEffectSource {
        id: repeat
        sourceItem: text
        smooth: true
        wrapMode: ShaderEffectSource.Repeat
    }

    ShaderEffect {
        anchors.fill: parent

        property variant cyan: hRepeat
        property variant magenta: vRepeat
        property variant yellow: repeat
        property variant black: clamp

        fragmentShader: "
            uniform lowp sampler2D cyan;
            uniform lowp sampler2D magenta;
            uniform lowp sampler2D yellow;
            uniform lowp sampler2D black;
            varying highp vec2 qt_TexCoord0;
            uniform lowp float qt_Opacity;
            void main() {
                highp vec2 t = qt_TexCoord0 * 3. - 1.;
                lowp float c = texture2D(cyan, t + vec2(.05, .09)).a;
                lowp float m = texture2D(magenta, t + vec2(.04, -.10)).a;
                lowp float y = texture2D(yellow, t + vec2(-.10, .01)).a;
                lowp float k = texture2D(black, t).a;
                gl_FragColor = 1. - vec4(c + k, m + k, y + k, 0.);
            }
            "
    }
}
