import QtQuick 2.0

Item {
    property int a: 0
    property int b: 0

    Component.onCompleted: {
        for (var i = ((a > b) ? b : a); i < ((a > b) ? a : b); i++)
        {
        }
    }
}
