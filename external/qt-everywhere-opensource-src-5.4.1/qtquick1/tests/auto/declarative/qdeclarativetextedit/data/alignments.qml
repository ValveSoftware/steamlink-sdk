import QtQuick 1.0

Rectangle {
    id: top
    width: 70; height: 70;

    property alias horizontalAlignment: t.horizontalAlignment
    property alias verticalAlignment: t.verticalAlignment
    property alias wrapMode: t.wrapMode
    property alias running: timer.running
    property string txt: "Test"

    Rectangle {
        anchors.centerIn: parent
        width: 40
        height: 40
        color: "green"

        TextEdit {
            id: t

            anchors.fill: parent
            horizontalAlignment: TextEdit.AlignRight
            verticalAlignment: TextEdit.AlignBottom
            wrapMode: TextEdit.WordWrap
            text: top.txt
        }
        Timer {
            id: timer

            interval: 1
            running: true
            repeat: true
            onTriggered: {
                top.txt = top.txt + "<br>more " + top.txt.length;
                if (top.txt.length > 50)
                    running = false
            }
        }
    }
}
