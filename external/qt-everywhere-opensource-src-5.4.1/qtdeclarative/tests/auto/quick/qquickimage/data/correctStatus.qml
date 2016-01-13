import QtQuick 2.0

Item {
    property alias status: image1.status

    Image {
        id: image1
        asynchronous: true
        source: "image://test/first-image.png"
    }

    Image {
        id: image2
        asynchronous: true
        source: "image://test/first-image.png"
    }

    Timer {
        interval: 50
        running: true
        repeat: false
        onTriggered: {
            image1.source = "image://test/second-image.png"
        }
    }
}
