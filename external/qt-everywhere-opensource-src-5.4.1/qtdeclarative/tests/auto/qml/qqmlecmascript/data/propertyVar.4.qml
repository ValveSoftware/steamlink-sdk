import QtQuick 2.0

Item {
    property bool test: false

    property var items: [1, 2, 3, "four", "five"]
    property int bound: items[0]

    Component.onCompleted: {
        if (bound != 1) return;

        items[0] = 10      // bound should remain 1

        if (bound != 1) return;

        test = true;
    }
}
