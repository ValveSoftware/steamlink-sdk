import QtQuick 2.0
import QtQuick.Window 2.0 as Window
import Test 1.0

Window.Window {

    width: 100
    height: 100

    RootItemAccessor {
        id:accessor
        objectName:"accessor"
        Component.onCompleted:accessor.contentItem();
    }
}
