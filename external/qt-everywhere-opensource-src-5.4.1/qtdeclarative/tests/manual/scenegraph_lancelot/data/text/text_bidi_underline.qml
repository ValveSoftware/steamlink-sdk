import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        anchors.fill: parent
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.family: "Arial"
        font.pixelSize: 20
        font.underline: true
        text: "Lorem ipsum dolor sit amet, agam sonet vitae. "
         + "جُل أم تشرشل والنازي. عل وقد كنقطة الهجوم. "
         + "Dolorum assueverit vis ex. Zril graeci eirmod sed."
    }
}

