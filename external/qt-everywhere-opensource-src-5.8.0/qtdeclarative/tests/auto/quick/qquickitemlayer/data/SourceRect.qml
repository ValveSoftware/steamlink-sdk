import QtQuick 2.0

Item
{
    width: 200
    height: 100

    Rectangle {
        id: box
        width: 200
        height: 100

        color: "#ff0000"

        layer.enabled: true
        layer.sourceRect: Qt.rect(-10, -10, box.width + 20, box.height + 20);

        // A shader that pads the transparent pixels with blue.
        layer.effect: ShaderEffect {
            fragmentShader: "
            uniform lowp sampler2D source;
            uniform lowp float qt_Opacity;
            varying highp vec2 qt_TexCoord0;
            void main() {
                mediump vec4 c = texture2D(source, qt_TexCoord0);
                if (c.a == 0.0)
                    c = vec4(0, 0, 1, 1);
                gl_FragColor = c * qt_Opacity;
            }
            "
        }
    }
}
