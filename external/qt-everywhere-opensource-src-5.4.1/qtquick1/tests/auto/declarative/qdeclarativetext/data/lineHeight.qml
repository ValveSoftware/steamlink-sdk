import QtQuick 1.1

Item {
    width: 200
    height: 200

    Text {
        id: myText
        objectName: "myText"
        width: 200
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        text: "Lorem ipsum sit amet, consectetur adipiscing elit. Integer felis nisl, varius in pretium nec, venenatis non erat. Proin lobortis interdum dictum."
    }
}
