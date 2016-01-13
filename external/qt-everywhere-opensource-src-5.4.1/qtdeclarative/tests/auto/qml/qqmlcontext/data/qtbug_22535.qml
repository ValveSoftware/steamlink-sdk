import QtQuick 2.0

Rectangle {
    width: 64
    height: 64

    Object_22535 { id:o }

    Component.onDestruction: o.goodBye()
}
