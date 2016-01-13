import QtQuick 2.0

Item {
    property int a: 5
    property int d: 12
    signal trigger(int b, int c)

    onAChanged: {
        trigger(a*2, 15+a)
    }

    function someFunc(b, c) {
        d = b+c;
    }

    Component.onCompleted: {
        trigger.connect(someFunc);
    }
}
