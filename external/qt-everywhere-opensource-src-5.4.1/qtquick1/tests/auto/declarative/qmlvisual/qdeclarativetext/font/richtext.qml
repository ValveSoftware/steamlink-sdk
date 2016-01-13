import QtQuick 1.0
import "../../shared" 1.0

Rectangle {
    id: s; width: 620; height: 300; color: "lightsteelblue"
    property string text: "<b>The</b> <i>quick</i> <u>brown</u> <o>fox</o> <big>jumps</big> <small>over</small> <tt>the</tt> <s>lazy</s> <em>dog</em>."

    Column {
        spacing: 6
        TestText {
            text: s.text
        }
        TestText {
            text: s.text; font.pixelSize: 18
        }
        TestText {
            text: s.text; font.pixelSize: 24
        }
        TestText {
            text: s.text; color: "red"; smooth: true
        }
        TestText {
            text: s.text; font.capitalization: "AllUppercase"
        }
        TestText {
            text: s.text; font.underline: true
        }
        TestText {
            text: s.text; font.overline: true; smooth: true
        }
        TestText {
            text: s.text; font.strikeout: true
        }
        TestText {
            text: s.text; font.underline: true; font.overline: true; font.strikeout: true
        }
        TestText {
            text: s.text; font.letterSpacing: 2
        }
        TestText {
            text: s.text; font.underline: true; font.letterSpacing: 2; font.capitalization: "AllUppercase"; color: "blue"
        }
        TestText {
            text: s.text; font.overline: true; font.wordSpacing: 25; font.capitalization: "Capitalize"; color: "green"
        }
    }
}
