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

    Component { id: a; QtObject{} }
    Component { id: b; Item{} }
    Component { id: c; Window{} }
    Component.onCompleted: {
        qtobject_qtobject = a.createObject(qtobjectParent);
        qtobject_item = b.createObject(qtobjectParent);
        qtobject_window = c.createObject(qtobjectParent);
        item_qtobject = a.createObject(itemParent);
        item_item = b.createObject(itemParent);
        item_window = c.createObject(itemParent);
        window_qtobject = a.createObject(windowParent);
        window_item = b.createObject(windowParent);
        window_window = c.createObject(windowParent);
    }
}
