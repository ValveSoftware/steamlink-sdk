import org.qtproject.PureJsModule 1.5 as PJM_1_5
import org.qtproject.PureJsModule 1.6 as PJM_1_6
import QtQuick 2.0

Item {
    property bool test: false

    Component.onCompleted: {
        test = ((PJM_1_5.FirstAPI.greeting() == "Hello") &&
                (PJM_1_5.FirstAPI.major == 1) &&
                (PJM_1_5.FirstAPI.minor == 0) &&
                (PJM_1_5.SecondAPI.greeting() == "Howdy") &&
                (PJM_1_5.SecondAPI.major == 1) &&
                (PJM_1_5.SecondAPI.minor == 5) &&
                (PJM_1_6.FirstAPI.greeting() == "Good news, everybody!") &&
                (PJM_1_6.FirstAPI.major == 1) &&
                (PJM_1_6.FirstAPI.minor == 6))
    }
}
