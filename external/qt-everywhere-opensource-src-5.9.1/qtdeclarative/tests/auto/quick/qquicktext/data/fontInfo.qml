import QtQuick 2.9

Item {
    Text {
        id: main
        objectName: "main"
        width: 500
        height: 500
        text: "Meaningless text"
        font.pixelSize: 1000
        fontSizeMode: Text.Fit
    }

    Text {
        objectName: "copy"
        text: main.text
        width: main.width
        height: main.height

        font.family: main.fontInfo.family
        font.pixelSize: main.fontInfo.pixelSize
    }
}

