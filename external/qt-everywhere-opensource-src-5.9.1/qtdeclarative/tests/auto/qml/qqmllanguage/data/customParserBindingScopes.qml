import Test 1.0
import QtQml 2.0
QtObject {
    id: root

    property int otherProperty: 10

    property QtObject child: QtObject {
        id: child

        property int testProperty;
        property int otherProperty: 41

        property QtObject customBinder: CustomBinding {
            target: child
            testProperty: otherProperty + 1
        }
    }
}
