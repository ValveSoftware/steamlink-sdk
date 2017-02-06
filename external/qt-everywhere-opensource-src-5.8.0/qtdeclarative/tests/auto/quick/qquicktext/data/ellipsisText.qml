import QtQuick 2.0

Text {
    width: 0
    height: 10
    text: "Meaningless text"
    elide: Text.ElideRight

    Text {
        objectName: "elidedRef"
        width: 10
        height: 10
        text: "Meaningless text"
        elide: Text.ElideRight
    }
}

