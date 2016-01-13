import QtQuick 1.0

Item {
    id: root

    property bool test: false

    property real testData: 9
    property real testData2: 9

    states: State {
        name: "change"
        PropertyChanges {
            target: myType
            realProperty: if (testData2 > 3) 9; else 11;
        }
    }

    AliasBindingsAssignCorrectlyType {
        id: myType

        aliasProperty: if (testData > 3) 14; else 12;
    }

    Component.onCompleted: {
        // Check original binding works
        if (myType.aliasProperty != 14) return;

        testData = 2;
        if (myType.aliasProperty != 12) return;

        // Change binding indirectly by modifying the "realProperty"
        root.state = "change";
        if (myType.aliasProperty != 9) return;

        // Check the new binding works
        testData2 = 1;
        if (myType.aliasProperty != 11) return;

        // Try and trigger the old binding (that should have been removed)
        testData = 6;
        if (myType.aliasProperty != 11) return;

        // Restore the original binding
        root.state = "";
        if (myType.aliasProperty != 14) return;

        // Test the restored binding works
        testData = 0;
        if (myType.aliasProperty != 12) return;

        // Test the old binding isn't somehow hanging around and still in effect
        testData2 = 13;
        if (myType.aliasProperty != 12) return;

        test = true;
    }
}

