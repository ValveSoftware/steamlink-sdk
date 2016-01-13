// test Thai font rendering and line breaking

import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.fill: parent
        wrapMode: Text.Wrap
        font.family: "Arial"
        font.pixelSize: 20
        text: "ภาษาไทย เป็นภาษาราชการของประเทศไทย และภาษาแม่ของชาวไทย และชนเชื้อสายอื่นในประเทศไทย ภาษาไทยเป็นภาษาในกลุ่มภาษาไท"
    }
}


