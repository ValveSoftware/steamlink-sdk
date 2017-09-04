import QtQuick 2.0
import "singleton/subdir"

Item {
    property int value1: ErrorSingletonType.testProp1;
    property string value2: "Test value: " + ErrorSingletonType.testProp3;
    property variant singletonInstance: ErrorSingletonType;
}
