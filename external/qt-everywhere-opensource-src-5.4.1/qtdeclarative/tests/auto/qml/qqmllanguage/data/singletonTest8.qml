import QtQuick 2.0
import org.qtproject.SingletonTest 2.5 as TestNameSpace

Item {
    property int value1: TestNameSpace.SingletonType.testProp1;
    property string value2: "Test value: " + TestNameSpace.SingletonType.testProp3;
    property variant singletonInstance: TestNameSpace.SingletonType;

    TestNameSpace.NonSingletonType { id: nonSingleton }

    property int value3: nonSingleton.testProp1;
    property string value4: "Test value: " + nonSingleton.testProp3;
}

