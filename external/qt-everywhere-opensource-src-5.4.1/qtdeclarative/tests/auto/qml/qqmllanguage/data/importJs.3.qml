import org.qtproject.PureJsModule 1.0
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = ((FirstAPI.greeting() == "Hello") &&
                (FirstAPI.major == 1) &&
                (FirstAPI.minor == 0) &&
                (SecondAPI.greeting() == "Howdy") &&
                (SecondAPI.major == 1) &&
                (SecondAPI.minor == 5))

    }
}
