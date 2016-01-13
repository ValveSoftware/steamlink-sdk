import QtQuick 2.0
import "singleton"

Item {
    property int value1: SingletonType.testProp1;
    property string value2: "Test value: " + SingletonType.testProp3;
}
