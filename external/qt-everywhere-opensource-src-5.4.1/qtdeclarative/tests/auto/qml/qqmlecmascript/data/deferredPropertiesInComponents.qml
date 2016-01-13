import Qt.test 1.0
import QtQml 2.0

QtObject {
    id: root
    property int value: 10
    property QtObject deferredInside: MyDeferredComponent {
                                          target: root
                                      }
    property QtObject deferredOutside: MyDeferredComponent2 {
                                           objectProperty: MyQmlObject {
                                               value: root.value
                                           }
                                       }
}
