import QtQuick 1.0
import "../../shared" 1.0

Rectangle {
    id: s; width: 600; height: 100;
    property string text: "The quick brown fox jumps over the lazy dog."
    TestText {
        text: s.text
        anchors.verticalCenter: s.verticalCenter
    }
    TestText {
        text: s.text
        anchors.baseline: s.verticalCenter
    }
}
