import QtQuick 2.0
import Test 1.0

TestObject {
    property int value1: 10
    property int value2: 5
    scriptString: value1 + value2
    scriptStringError: value3 * 5
}
