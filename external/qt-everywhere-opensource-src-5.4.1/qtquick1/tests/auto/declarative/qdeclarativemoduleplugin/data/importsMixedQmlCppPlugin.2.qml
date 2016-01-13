import org.qtproject.AutoTestQmlMixedPluginType 1.5
import QtQuick 1.0

Item {
    property bool test: false
    property bool test2: false

    Bar {
        id: bar
    }

    Foo {
        id: foo
    }

    Component.onCompleted: {
        test = (bar.value == 16);
        test2 = (foo.value == 89);
    }
}

