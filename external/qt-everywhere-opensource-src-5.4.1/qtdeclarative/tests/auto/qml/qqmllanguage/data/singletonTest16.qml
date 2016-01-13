import QtQuick 2.0
import "singleton"
import "singleton/js/jspragma.js" as JsPragmaTest

Item {
    id: test

    property int value1: SingletonType.testProp1;
    property string value2: "Test value: " + JsPragmaTest.value;
}
