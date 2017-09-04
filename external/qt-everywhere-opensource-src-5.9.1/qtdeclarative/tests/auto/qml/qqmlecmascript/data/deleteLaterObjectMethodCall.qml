import QtQuick 2.0
import Qt.test 1.0

QtObject {
    property var fn

    property var c: Component {
        MyQmlObject {
            function go() {
                try { methodNoArgs(); } catch(e) { }
            }
        }
    }

    Component.onCompleted: {
        var f = c.createObject().go;

        gc();

        f();
    }
}
