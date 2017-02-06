import QtQuick 2.0

QtObject {
    Component.onCompleted: {
        var t = tp.time;
        tp.time = t;
    }
}
