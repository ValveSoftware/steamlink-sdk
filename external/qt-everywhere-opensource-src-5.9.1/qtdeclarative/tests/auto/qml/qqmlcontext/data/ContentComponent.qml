import QtQuick 2.0

Item {
    property int count: 0
    property bool hasValidParent: parent && parent.children.length != 0

    onHasValidParentChanged: {
        if (++count > 1) parent.doSomething()
    }
}
