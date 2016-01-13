import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    color: "skyblue"
    ShaderEffect{
        anchors.centerIn: parent
        width: 16 * 16
        height: 24 * 16
        property variant source: ShaderEffectSource {
            sourceItem: Rectangle {
                width: 22 * 20
                height: 16 * 20
                color: "#EF2B2D"
                Rectangle {
                    y: 6 * 20
                    height: 4 * 20
                    width: 22 * 20
                    color: "white"
                }
                Rectangle {
                    x: 6 * 20
                    width: 4 * 20
                    height: 16 * 20
                    color: "white"
                }
                Rectangle {
                    y: 7 * 20
                    height: 2 * 20
                    width: 22 * 20
                    color: "#002868"
                }
                Rectangle {
                    x: 7 * 20
                    width: 2 * 20
                    height: 16 * 20
                    color: "#002868"
                }
            }
            smooth: true
        }
        vertexShader: "
            uniform highp mat4 qt_Matrix;
            attribute highp vec4 qt_Vertex;
            attribute highp vec2 qt_MultiTexCoord0;
            varying highp vec2 qt_TexCoord0;
            void main() {
                highp vec4 pos = qt_Vertex;
                pos.x += sin(qt_Vertex.y * 0.02) * 20.;
                pos.y += sin(qt_Vertex.x * 0.02) * 20.;
                gl_Position = qt_Matrix * pos;
                qt_TexCoord0 = qt_MultiTexCoord0;
            }"
        mesh: GridMesh {
            property int r: 2
            resolution: Qt.size(r, r)
        }
    }
}
