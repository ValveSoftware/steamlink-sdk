import QtQuick 2.0

Item {
    property bool test: false

    property var attributes: { 'color': 'red', 'width': 100 }
    property int bound: attributes.width

    Component.onCompleted: {
        if (bound != 100) return;

        attributes = { 'color': 'blue', 'width': 200 }   // bound should now be 200

        if (bound != 200) return;

        test = true;
    }
}
