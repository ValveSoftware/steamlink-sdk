import QtQuick 2.0

Item {
    property int a: 5
    property int b: 0

    function someFunc() {
        b += a;
    }

    Component.onCompleted: {
        onAChanged.connect(someFunc)
    }
}
