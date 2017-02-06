import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    color: "black"

    Text {
        id: text
        anchors.fill: parent
        font.pixelSize: 16
        text: "In the field of computer graphics, a shader is a set of software instructions which"
            + " is used primarily to calculate rendering effects on graphics hardware with a high "
            + "degree of flexibility. Shaders are used to program the graphics processing unit (GP"
            + "U) programmable rendering pipeline, which has mostly superseded the fixed-function "
            + "pipeline that allowed only common geometry transformation and pixel-shading functio"
            + "ns; with shaders, customized effects can be used."
        wrapMode: Text.Wrap
        color: "yellow"
        visible: false
    }

    ShaderEffectSource {
        anchors.fill: parent
        sourceItem: text
        smooth: true
        mipmap: true
        scale: 0.6
    }
}
