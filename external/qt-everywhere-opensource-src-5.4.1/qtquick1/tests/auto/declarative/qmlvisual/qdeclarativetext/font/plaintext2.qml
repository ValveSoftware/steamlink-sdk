import QtQuick 1.0
import "../../shared" 1.0

Rectangle {
    width: 400; height: 200

    Row {
        spacing: 20
        anchors.centerIn: parent
        TestText {
            text: "First line\nSecond line"; wrapMode: Text.Wrap
        }
        TestText {
            text: "First line\nSecond line"; width: 70
        }
        TestText {
            text: "First Second\nThird Fourth"; wrapMode: Text.Wrap; width: 50
        }
        TestText {
            text: "First line<br>Second line"; textFormat: Text.StyledText
        }
    }
}
