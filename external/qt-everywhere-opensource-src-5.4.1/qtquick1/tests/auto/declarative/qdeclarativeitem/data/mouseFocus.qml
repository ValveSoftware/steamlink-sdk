import QtQuick 1.0

QGraphicsWidget {
    size: "200x100"
    focusPolicy: QGraphicsWidget.ClickFocus
    Item {
        objectName: "declarativeItem"
        id: item
        width: 200
        height: 100
        MouseArea {
            anchors.fill: parent
            onPressed: {
                if (!item.focus) {
                    item.focus = true;
                }
            }
        }
    }
}
