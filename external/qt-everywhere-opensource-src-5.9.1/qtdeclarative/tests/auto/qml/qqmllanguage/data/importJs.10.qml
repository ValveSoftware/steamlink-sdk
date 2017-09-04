import org.qtproject.PureJsModule 1.0 as PJM
import org.qtproject.PureJsModule 1.0 as AnotherName
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = ((PJM.FirstAPI.greeting() == "Hello") &&
                (PJM.FirstAPI.major == 1) &&
                (PJM.FirstAPI.minor == 0) &&
                (AnotherName.FirstAPI.greeting() == "Hello") &&
                (AnotherName.FirstAPI.major == 1) &&
                (AnotherName.FirstAPI.minor == 0))
    }
}
