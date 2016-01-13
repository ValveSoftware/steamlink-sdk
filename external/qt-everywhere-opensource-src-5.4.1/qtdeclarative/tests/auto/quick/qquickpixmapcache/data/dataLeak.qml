import QtQuick 2.0

Item {
    id: root
    width: 800
    height: 800

    Image {
        id: i1
        source: "exists1.png";
        anchors.top: parent.top;
    }
    Image {
        id: i2
        source: "exists2.png"
        anchors.top: i1.bottom;
    }
}
