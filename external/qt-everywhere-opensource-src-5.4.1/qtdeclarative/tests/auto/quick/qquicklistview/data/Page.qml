import QtQuick 2.0

Item {
    anchors.fill: parent
    default property alias contentArea: contentItem.data
    Item {
        id: contentItem
        anchors.fill: parent
    }
}
