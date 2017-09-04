import QtQuick 2.0
import "singleton"

Item {
    id: test

    property int value1: SingletonType.testProp1;
    property string value2: "Test value: " + SingletonType.testProp3;

    signal customSignal(SingletonType type)

    onCustomSignal: {
        type.testProp1 = 99
    }

    Component.onCompleted: test.customSignal(SingletonType)
}
