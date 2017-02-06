import QtQuick 2.0

Item {
    property bool test: false

    property var attributes: { 'color': 'red', 'width': 100 }
    property int bound: attributes.width

    Component.onCompleted: {
        if (bound != 100) return;

        attributes.width = 200   // bound should remain 100

        if (bound != 100) return;

        test = true;
    }
}
