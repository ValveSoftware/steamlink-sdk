import QtQuick 2.0

Item {
    width: 300
    height: 200

    Text {
        id: myText
        objectName: "myText"
        elide: Text.ElideRight
        maximumLineCount: 2
        width: 200
        wrapMode: Text.WordWrap
        text: "This is a sad face aligned to the top. Lorem ipsum dolor sit amet. Nulla sed turpis risus. Integer sit amet odio quis mauris varius venenatis<img src=\"images/face-sad.png\" width=\"30\" height=\"30\" align=\"top\">Lorem ipsum dolor sit amet. Nulla sed turpis risus. Integer sit amet odio quis mauris varius venenatis. Lorem ipsum dolor sit amet. Nulla sed turpis risus.Lorem ipsum dolor sit amet. Nulla sed turpis risus. Lorem ipsum dolor sit amet. Nulla sed turpis risus.Lorem ipsum dolor sit amet. Nulla sed turpis risus."
    }

    MouseArea {
        anchors.fill: parent
        onClicked: myText.width = 400

    }
}


