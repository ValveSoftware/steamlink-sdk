import QtQuick 2.0

Item {
    property bool test: false

    property var literalValue: 6

    Component.onCompleted: {
        if (literalValue != 6) return;
        test = true;
    }
}
