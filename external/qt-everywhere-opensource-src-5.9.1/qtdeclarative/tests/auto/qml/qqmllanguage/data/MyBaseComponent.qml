import QtQuick 2.0

QtObject {
    id: base

    property bool baseSuccess: false

    property string baseProperty: 'foo'
    property string boundProperty: baseProperty
    property alias aliasProperty: base.baseProperty

    function basePropertiesTest(expected) {
        return (baseProperty == expected &&
                boundProperty == expected &&
                aliasProperty == expected);
    }

    Component.onCompleted: {
        if (basePropertiesTest('foo')) {
            baseProperty = 'bar';
            baseSuccess = basePropertiesTest('bar');
        }
    }
}
