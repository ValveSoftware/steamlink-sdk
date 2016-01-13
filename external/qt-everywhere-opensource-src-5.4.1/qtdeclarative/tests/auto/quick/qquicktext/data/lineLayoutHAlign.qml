import QtQuick 2.0

Item {
    id: main
    width: 200; height: 200

    Text {
        id: myText
        objectName: "myText"
        width: parent.width
        font.family: "__Qt__Box__Engine__"
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit."

        onLineLaidOut: {
            // do nothing
        }
    }
}
