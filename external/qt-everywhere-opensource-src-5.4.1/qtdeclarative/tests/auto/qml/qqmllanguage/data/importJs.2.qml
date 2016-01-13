import org.qtproject.VersionedOnlyJsModule 9.0
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = ((SomeAPI.greeting() == "Hey hey hey") &&
                (SomeAPI.major == 9) &&
                (SomeAPI.minor == 0))
    }
}
