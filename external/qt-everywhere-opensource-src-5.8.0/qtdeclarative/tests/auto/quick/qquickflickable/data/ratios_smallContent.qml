import QtQuick 2.0

Flickable {
    property double heightRatioIs: visibleArea.heightRatio
    property double widthRatioIs: visibleArea.widthRatio

    width: 200
    height: 200
    contentWidth: item.width
    contentHeight: item.height
    topMargin: 20
    leftMargin: 40

    Item {
        id: item
        width: 100
        height: 100
    }
}
