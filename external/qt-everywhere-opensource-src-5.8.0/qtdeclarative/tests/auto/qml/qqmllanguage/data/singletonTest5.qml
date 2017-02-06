import QtQuick 2.0
import "singleton" as TestNameSpace

Item {
    property int value1: TestNameSpace.SingletonType.testProp1;
    property string value2: "Test value: " + TestNameSpace.SingletonType.testProp3;
    property variant singletonInstance: TestNameSpace.SingletonType;
}
