import QtQml 2.0
import Qt.test.singletonWithEnum 1.0

QtObject {
    property int testValue: 0
    Component.onCompleted: {
        testValue = SingletonWithEnum.TestValue;
    }
}
