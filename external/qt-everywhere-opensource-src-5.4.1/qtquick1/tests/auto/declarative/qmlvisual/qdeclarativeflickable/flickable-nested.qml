import QtQuick 1.0

Item {
    width: 640
    height: 400

    Flickable {
        objectName: "flick 1"
        anchors.fill: parent
        contentWidth: width + 100
        contentHeight: height + 100

        Rectangle {
            width: 300
            height: 300
            color: "blue"

            Flickable {
                objectName: "flick 2"
                width: 300
                height: 300
                clip: true
                contentWidth: 400
                contentHeight: 400

                Rectangle {
                    width: 100
                    height: 100
                    anchors.centerIn: parent
                    color: "yellow"

                    Flickable {
                        objectName: "flick 3"
                        anchors.fill: parent
                        clip: true
                        contentWidth: 150
                        contentHeight: 150
                        Rectangle {
                            x: 80
                            y: 80
                            width: 50
                            height: 50
                            color: "green"
                        }
                    }
                }
            }
        }
    }
}
