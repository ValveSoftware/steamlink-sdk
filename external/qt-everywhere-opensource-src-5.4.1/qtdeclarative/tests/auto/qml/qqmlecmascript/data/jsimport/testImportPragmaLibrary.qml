import QtQuick 2.0

// We use the components specified in SpecialRectangleOne.qml and SpecialRectangleTwo.qml

QtObject {
    id: testQtObject

    property SpecialRectangleOne a;
    property SpecialRectangleTwo b;

    a: SpecialRectangleOne {
        id: rectangleOne
    }
    b: SpecialRectangleTwo {
        id: rectangleTwo
    }

    // this should be: (5 + 15) + (6 + 5) == 31
    property int testValue: rectangleOne.height + rectangleTwo.height
}
