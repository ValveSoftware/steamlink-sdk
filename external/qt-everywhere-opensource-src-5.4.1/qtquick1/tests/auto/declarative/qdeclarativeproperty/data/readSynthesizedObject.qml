import QtQuick 1.0

QtObject {
    property TestType test

    test: TestType {
        property int b: 19
    }
}
