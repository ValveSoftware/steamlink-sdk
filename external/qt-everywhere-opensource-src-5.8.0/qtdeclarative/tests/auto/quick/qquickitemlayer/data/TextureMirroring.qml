import QtQuick 2.6

Item
{
    width: 250
    height: 50

    property int mirroring: 0

    // Layered box without effect. Mirroring should not affect how it looks.
    Rectangle {
        x: 0
        y: 0
        width: 50
        height: 50
        layer.enabled: true
        layer.textureMirroring: mirroring
        Rectangle {
            x: 0
            y: 0
            width: 25
            height: 25
            color: "#000000"
        }
        Rectangle {
            x: 25
            y: 0
            width: 25
            height: 25
            color: "#ff0000"
        }
        Rectangle {
            x: 0
            y: 25
            width: 25
            height: 25
            color: "#00ff00"
        }
        Rectangle {
            x: 25
            y: 25
            width: 25
            height: 25
            color: "#0000ff"
        }
    }

    // Layered box with effect. Mirroring should affect how it looks.
    Rectangle {
        id: layeredEffectBox
        x: 50
        y: 0
        width: 50
        height: 50
        layer.enabled: true
        layer.textureMirroring: mirroring
        layer.samplerName: "source"
        layer.effect: ShaderEffect {
            property variant source: layeredEffectBox
            fragmentShader: "
                uniform lowp sampler2D source;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_FragColor = texture2D(source, qt_TexCoord0);
                }"

        }

        Rectangle {
            x: 0
            y: 0
            width: 25
            height: 25
            color: "#000000"
        }
        Rectangle {
            x: 25
            y: 0
            width: 25
            height: 25
            color: "#ff0000"
        }
        Rectangle {
            x: 0
            y: 25
            width: 25
            height: 25
            color: "#00ff00"
        }
        Rectangle {
            x: 25
            y: 25
            width: 25
            height: 25
            color: "#0000ff"
        }
    }

    // Non-layered source item for ShaderEffectSource. Mirroring should not affect how it looks.
    Rectangle {
        id: box2
        x: 100
        y: 0
        width: 50
        height: 50
        Rectangle {
            x: 0
            y: 0
            width: 25
            height: 25
            color: "#000000"
        }
        Rectangle {
            x: 25
            y: 0
            width: 25
            height: 25
            color: "#ff0000"
        }
        Rectangle {
            x: 0
            y: 25
            width: 25
            height: 25
            color: "#00ff00"
        }
        Rectangle {
            x: 25
            y: 25
            width: 25
            height: 25
            color: "#0000ff"
        }
    }
    // ShaderEffectSource item. Mirroring should not affect how it looks.
    ShaderEffectSource {
        id: theSource
        x: 150
        y: 0
        width: 50
        height: 50
        sourceItem: box2
        textureMirroring: mirroring
    }
    // ShaderEffect item. Mirroring should affect how it looks.
    ShaderEffect {
        x: 200
        y: 0
        width: 50
        height: 50
        property variant source: theSource
        fragmentShader: "
            uniform lowp sampler2D source;
            varying highp vec2 qt_TexCoord0;
            void main() {
                gl_FragColor = texture2D(source, qt_TexCoord0);
            }"
    }
}
