import QtQuick 2.1
import QtQuick.Window 2.1

Window {
    width: 240
    height: 75
    title: "Hammer sez"
    color: "#e0c31e"
    property bool canCloseThis: false;
    Rectangle {
        color: "#14148c"
        width: parent.width * 0.85
        height: parent.height * 0.7
        anchors.horizontalCenter: parent.horizontalCenter
        Text {
            id: text
            anchors.centerIn: parent
            color: "white"
            textFormat: Text.StyledText
            text: "whoa-oa-oa-oh<br/>U can't <font color='#b40000' size='+1'>close</font> this"
        }
    }
    onClosing: {
        if (canCloseThis) {
            text.text = "uncle! I give up"
            // the event is accepted by default
        } else {
            close.accepted = false
            text.text = "...but you still can't close this"
        }
    }
}
