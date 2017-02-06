import org.qtproject.PureJsModule 1.5 as PJM
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = ((PJM.FirstAPI.greeting() == "Hello") &&
                (PJM.FirstAPI.major == 1) &&
                (PJM.FirstAPI.minor == 0) &&
                (PJM.SecondAPI.greeting() == "Howdy") &&
                (PJM.SecondAPI.major == 1) &&
                (PJM.SecondAPI.minor == 5))
    }
}
