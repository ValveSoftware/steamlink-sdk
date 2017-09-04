import QtQuick 2.0

Item {
    id: root
    width: 800
    height: 800

    GridView {
        anchors.fill: parent
        delegate: Image {
            source: serverBaseUrl + imagePath;
            asynchronous: true
            smooth: true
            width: 200
            height: 200
        }
        model: ListModel {
            ListElement {
                imagePath: "/big256.png"
            }
            ListElement {
                imagePath: "/big256.png"
            }
            ListElement {
                imagePath: "/big256.png"
            }
            ListElement {
                imagePath: "/colors.png"
            }
            ListElement {
                imagePath: "/colors1.png"
            }
            ListElement {
                imagePath: "/big.jpeg"
            }
            ListElement {
                imagePath: "/heart.png"
            }
            ListElement {
                imagePath: "/green.png"
            }
        }
    }
}
