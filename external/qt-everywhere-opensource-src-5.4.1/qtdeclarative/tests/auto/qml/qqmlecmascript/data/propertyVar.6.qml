import QtQuick 2.0

Item {
    property bool test: false

    property var items: [1, 2, 3, "four", "five"]
    property int bound: items[0]
    property var funcs: [(function() { return 6; })]
    property int bound2: funcs[0]()

    function returnTwenty() {
        return 20;
    }

    Component.onCompleted: {
        if (bound != 1) return false;
        if (bound2 != 6) return false;

        items = [10, 2, 3, "four", "five"]  // bound should now be 10
        funcs = [returnTwenty]              // bound2 should now be 20

        if (bound != 10) return false;
        if (bound2 != 20) return false;

        test = true;
    }
}
