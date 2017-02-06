import org.qtproject.EmptyJsModule 1.0 as Empty
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = typeof Empty == 'object'
    }
}
