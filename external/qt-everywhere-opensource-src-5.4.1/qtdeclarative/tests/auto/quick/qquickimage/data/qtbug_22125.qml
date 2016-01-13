import QtQuick 2.0

Item {
    id: root
    width: 800
    height: 800

    GridView {
        anchors.fill: parent
        delegate: Image {
            source: imagePath;
            asynchronous: true
            smooth: true
            width: 200
            height: 200
        }
        model: ListModel {
            ListElement {
                imagePath: "http://127.0.0.1:14451/big256.png"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/big256.png"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/big256.png"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/colors.png"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/colors1.png"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/big.jpeg"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/heart.png"
            }
            ListElement {
                imagePath: "http://127.0.0.1:14451/green.png"
            }
        }
    }
}
