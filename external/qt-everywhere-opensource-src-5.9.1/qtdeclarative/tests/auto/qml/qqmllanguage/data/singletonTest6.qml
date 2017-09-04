import QtQuick 2.0
import org.qtproject.SingletonTest 1.0

Item {
    property int value1: SingletonType.testProp1;
    property string value2: "Test value: " + SingletonType.testProp3;
    property variant singletonInstance: SingletonType;

    NonSingletonType { id: nonSingleton }

    property int value3: nonSingleton.testProp1;
    property string value4: "Test value: " + nonSingleton.testProp3;
}

