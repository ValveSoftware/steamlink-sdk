import QtQuick 2.0

Item {
    id: root

    property RecursiveComponent myInner: RecursiveComponent {}
    property bool success: false

    Component.onCompleted: {
        success = (myInner.innermost != null)
    }
}
