import Test 1.0
import QtQuick 2.0
import QtQuick 2.0 as Qt47

QtObject {
    property QtObject objectProperty
    property QtObject objectProperty2
    objectProperty2: QtObject {}

    property MyComponent myComponentProperty
    property MyComponent myComponentProperty2
    myComponentProperty2: MyComponent {}
}
