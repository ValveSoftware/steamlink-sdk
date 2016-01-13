import QtQuick 2.0

QtObject {
    property int dummy: 13

    readonly property int test1: 19
    readonly property int test2: dummy * 49
    readonly property alias test3: other.test

    property bool test: false

    property var dummyObj: QtObject {
        id: other
        property int test: 9
    }

    Component.onCompleted: {
        if (test1 != 19) return;
        if (test2 != 637) return;
        if (test3 != 9) return;

        var caught = false;

        caught = false;
        try { test1 = 13 } catch (e) { caught = true; }
        if (!caught) return;

        caught = false;
        try { test2 = 13 } catch (e) { caught = true; }
        if (!caught) return;

        caught = false;
        try { test3 = 13 } catch (e) { caught = true; }
        if (!caught) return;

        other.test = 13;
        dummy = 9;

        if (test1 != 19) return;
        if (test2 != 441) return;
        if (test3 != 13) return;

        test = true;
    }
}
