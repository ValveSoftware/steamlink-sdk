import QtQuick 2.0

Item {
    property bool test: false
    property var nullOne: null
    property var nullTwo
    property var undefOne: undefined
    property var undefTwo

    Component.onCompleted: {
        nullTwo = null;
        undefTwo = undefined;
        if (nullOne != null) return;
        if (nullOne != nullTwo) return;
        if (undefOne != undefined) return;
        if (undefOne != undefTwo) return;
        test = true;
    }
}
