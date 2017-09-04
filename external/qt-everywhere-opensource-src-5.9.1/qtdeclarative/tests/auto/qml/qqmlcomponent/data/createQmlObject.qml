import QtQuick 2.0
import QtQuick.Window 2.0

Item {
    property QtObject qtobjectParent: QtObject { }
    property QtObject itemParent: Item { }
    property QtObject windowParent: Window { }

    property QtObject qtobject_qtobject : null
    property QtObject qtobject_item : null
    property QtObject qtobject_window : null

    property QtObject item_qtobject : null
    property QtObject item_item : null
    property QtObject item_window : null

    property QtObject window_qtobject : null
    property QtObject window_item : null
    property QtObject window_window : null

    Component.onCompleted: {
        qtobject_qtobject = Qt.createQmlObject("import QtQuick 2.0; QtObject{}", qtobjectParent);
        qtobject_item = Qt.createQmlObject("import QtQuick 2.0; Item{}", qtobjectParent);
        qtobject_window = Qt.createQmlObject("import QtQuick.Window 2.0; Window{}", qtobjectParent);
        item_qtobject = Qt.createQmlObject("import QtQuick 2.0; QtObject{}", itemParent);
        item_item = Qt.createQmlObject("import QtQuick 2.0; Item{}", itemParent);
        item_window = Qt.createQmlObject("import QtQuick.Window 2.0; Window{}", itemParent);
        window_qtobject = Qt.createQmlObject("import QtQuick 2.0; QtObject{}", windowParent);
        window_item = Qt.createQmlObject("import QtQuick 2.0; Item{}", windowParent);
        window_window = Qt.createQmlObject("import QtQuick.Window 2.0; Window{}", windowParent);
    }
}
