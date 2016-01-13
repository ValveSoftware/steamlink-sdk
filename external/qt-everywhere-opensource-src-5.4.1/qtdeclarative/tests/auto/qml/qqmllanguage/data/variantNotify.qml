import QtQuick 2.0

QtObject {
    property int notifyCount: 0

    property variant foo
    onFooChanged: notifyCount++

    Component.onCompleted: {
        foo = 1;
        foo = 1;
    }
}
