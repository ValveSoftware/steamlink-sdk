import QtQuick 2.0

QtObject {
    property int test

    Component.onCompleted: {
        var o = new Object;
        o.test = 92;
        test = o.test;
    }
}

