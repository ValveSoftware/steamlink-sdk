import QtQuick 2.0
import Qt.test 1.0

Item {
    property bool test: false

    MyQmlObject { id: itemsObj; qjsvalue: [1, 2, 3, "four", "five"] }
    MyQmlObject { id: funcsObj; qjsvalue: [(function() { return 6; })] }

    property alias items: itemsObj.qjsvalue
    property int bound: itemsObj.qjsvalue[0]
    property alias funcs: funcsObj.qjsvalue
    property int bound2: funcsObj.qjsvalue[0]()

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
