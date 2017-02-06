import Test 1.0
import QtQml 2.0
SimpleExtendedObjectWithCustomParser {
    id : obj
    intProperty: 42
    property string qmlString: "Hello"
    property var someObject: QtObject {}

    property int c : obj.extendedProperty

    function getExtendedProperty() { return c; }
}
