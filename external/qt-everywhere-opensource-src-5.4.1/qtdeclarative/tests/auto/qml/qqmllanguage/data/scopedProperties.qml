import QtQuick 2.0

MyBaseComponent {
    id: extended

    property bool success: false

    property int baseProperty: 666
    property int boundProperty: baseProperty
    property alias aliasProperty: extended.baseProperty

    function extendedPropertiesTest(expected) {
        return (baseProperty == expected &&
                boundProperty == expected &&
                aliasProperty == expected);
    }

    Component.onCompleted: {
        if (basePropertiesTest('bar') && extendedPropertiesTest(666)) {
            baseProperty = 999;
            success = extendedPropertiesTest(999) && baseSuccess;
        }
    }
}
