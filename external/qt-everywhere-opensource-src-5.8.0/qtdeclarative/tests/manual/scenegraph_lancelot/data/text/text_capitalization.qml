import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        id: upperCaseText
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        font.capitalization: Font.AllUppercase
        text: "This text is all uppercase"
    }

    Text {
        id: lowerCaseText
        anchors.top: upperCaseText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        font.family: "Arial"
        font.pixelSize: 16
        font.capitalization: Font.AllLowercase
        text: "This text is all lowercase"
    }

    Text {
        id: smallCapsText
        anchors.top: lowerCaseText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        font.family: "Arial"
        font.pixelSize: 16
        font.capitalization: Font.SmallCaps
        text: "This text is smallcaps"
    }

    Text {
        id: capitalizedText
        anchors.top: smallCapsText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        font.family: "Arial"
        font.pixelSize: 16
        font.capitalization: Font.Capitalize
        text: "This text is capitalized"
    }


}
