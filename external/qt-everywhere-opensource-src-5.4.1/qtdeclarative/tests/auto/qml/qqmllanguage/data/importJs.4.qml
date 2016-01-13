import org.qtproject.PureJsModule 1.6
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = ((FirstAPI.greeting() == "Good news, everybody!") &&
                (FirstAPI.major == 1) &&
                (FirstAPI.minor == 6) &&
                (SecondAPI.greeting() == "Howdy") &&
                (SecondAPI.major == 1) &&
                (SecondAPI.minor == 5))
    }
}
