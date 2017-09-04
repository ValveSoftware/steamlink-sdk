// test use of Fixedsys font on Windows

import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.fill: parent
        wrapMode: Text.Wrap
        font.family: "Fixedsys"
        font.pixelSize: 20
        text: "Foobar"
    }
}


