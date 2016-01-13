import QtQuick 2.0

Rectangle {
    id: root
    color:"red"
    property bool rootIndestructible:false
    property bool childDestructible:child === null
    onColorChanged: {
      try {
        root.destroy();
        gc();
      } catch(e) {
         rootIndestructible = true;
      }
    }

    QtObject {
      id:child
    }
    Component.onCompleted: child.destroy();
}