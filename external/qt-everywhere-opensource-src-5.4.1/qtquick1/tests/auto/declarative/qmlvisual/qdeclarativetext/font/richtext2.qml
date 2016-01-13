import QtQuick 1.0
import "../../shared" 1.0

//This is a continuation of richtext.qml, it was bisected so that it could fit on smaller screens
Rectangle {
    id: s; width: 620; height: 300; color: "lightsteelblue"
    property string text: "<b>The</b> <i>quick</i> <u>brown</u> <o>fox</o> <big>jumps</big> <small>over</small> <tt>the</tt> <s>lazy</s> <em>dog</em>."

    Column {
        spacing: 6
        TestText {
            text: s.text; font.pixelSize: 18; style: Text.Outline; styleColor: "white"
        }
        TestText {
            text: s.text; font.pixelSize: 18; style: Text.Sunken; styleColor: "gray"
        }
        TestText {
            text: s.text; font.pixelSize: 18; style: Text.Raised; styleColor: "yellow"
        }
        TestText {
            text: s.text; horizontalAlignment: Text.AlignLeft; width: s.width
        }
        TestText {
            text: s.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; width: s.width; height: 20
        }
        TestText {
            text: s.text; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignBottom; width: s.width; height: 20
        }
        Row{
            height: childrenRect.height;
            spacing: 4
            TestText {
                text: s.text + " thisisaverylongstringwithnospaces"; width: 150; wrapMode: Text.WrapAnywhere
            }
            TestText {
                text: s.text + " thisisaverylongstringwithnospaces"; width: 150; wrapMode: Text.Wrap
            }
            TestText {
                text: s.text; font.pixelSize: 18; style: Text.Outline; styleColor: "white"; wrapMode: Text.WordWrap; width: 200
            }
        }
    }
}
