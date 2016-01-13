import Test 1.0
import QtQuick 1.0 as MyQt

MyTypeObject {
    MyQt.Component.onCompleted: {
        font.capitalization = MyQt.Font.AllUppercase
    }
}


