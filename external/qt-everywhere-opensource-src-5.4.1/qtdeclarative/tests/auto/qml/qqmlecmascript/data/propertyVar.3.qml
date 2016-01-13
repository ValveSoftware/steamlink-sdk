import QtQuick 2.0

Item {
    id: root
    property bool test: false

    property var jsint: 4
    property int bound: jsint + 5

    Component.onCompleted: {
        if (bound != 9) return;

        jsint = jsint + 1;

        if (bound != 10) return;

        test = true;
    }
}
