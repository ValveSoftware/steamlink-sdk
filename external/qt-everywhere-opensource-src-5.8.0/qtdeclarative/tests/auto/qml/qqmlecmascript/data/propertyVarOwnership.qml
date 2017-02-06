import QtQuick 2.0
import Qt.test 1.0

Item {
    id: testOwnership
    property bool test: false

    property var varProperty

    function runTest() {
        if (varProperty != undefined) return;
        varProperty = { a: 10, b: 11 }
        if (varProperty.a != 10) return;

        gc(); // Shouldn't collect

        if (varProperty.a != 10) return;

        test = true;
    }
}

