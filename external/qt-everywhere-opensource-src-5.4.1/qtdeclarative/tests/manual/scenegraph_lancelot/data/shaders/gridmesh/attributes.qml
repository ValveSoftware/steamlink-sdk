import QtQuick 2.0

Rectangle {
    width: 320
    height: 480

    Text {
        id: text
        font.pixelSize:  80
        text: "Shaderz!"
    }

    ShaderEffectSource {
        id: source
        sourceItem: text
        hideSource: true
        smooth: true
    }
    Column {
        ShaderEffect {
            width: 320
            height: 160
            property variant source: source
            vertexShader: "
                uniform highp mat4 qt_Matrix;
                attribute highp vec4 qt_Vertex;
                attribute highp vec2 qt_MultiTexCoord0;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_Position = qt_Matrix * qt_Vertex;
                    qt_TexCoord0 = qt_MultiTexCoord0;
                }"
        }
        ShaderEffect {
            width: 320
            height: 160
            property variant source: source
            vertexShader: "
                attribute highp vec2 qt_MultiTexCoord0;
                uniform highp mat4 qt_Matrix;
                attribute highp vec4 qt_Vertex;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_Position = qt_Matrix * qt_Vertex;
                    qt_TexCoord0 = qt_MultiTexCoord0;
                }"
        }
        ShaderEffect {
            width: 320
            height: 160
            property variant source: source
            vertexShader: "
                attribute highp vec2 qt_MultiTexCoord0;
                uniform highp mat4 qt_Matrix;
                attribute highp vec4 qt_Vertex;
                varying highp vec2 qt_TexCoord0;
                uniform highp float width;
                uniform highp float height;
                void main() {
                    gl_Position = qt_Matrix * qt_Vertex;
                    qt_TexCoord0 = qt_Vertex.xy / vec2(width, height);
                }"
        }
    }
}
