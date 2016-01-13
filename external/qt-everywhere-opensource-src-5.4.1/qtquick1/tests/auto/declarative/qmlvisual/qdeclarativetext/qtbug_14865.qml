import QtQuick 1.0
import "../shared" 1.0

Rectangle {
    width: 100; height: 20

    TestText {
        id: label
        objectName: "label"
        text: "Hello world!"
        width: 10
    }

    Timer {
        running: true; interval: 1000
        onTriggered: label.text = ""
    }
}
