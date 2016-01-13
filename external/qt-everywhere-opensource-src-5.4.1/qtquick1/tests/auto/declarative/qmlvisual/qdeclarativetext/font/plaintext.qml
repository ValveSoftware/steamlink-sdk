import QtQuick 1.0
import "../../shared" 1.0

Rectangle {
    id: s; width: 620; height: 360; color: "lightsteelblue"
    property string text: "Jackdaws love my big sphinx of quartz."

    Column {
        spacing: 8
        TestText {
            text: s.text; horizontalAlignment: Text.AlignLeft; width: s.width
        }
        TestText {
            font.pixelSize: 18
            text: s.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; width: s.width;
        }
        TestText {
            font.pixelSize: 24
            text: s.text; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignBottom; width: s.width;
        }
        Grid{
            columns: 2
            spacing: 4
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
                text: s.text; style: Text.Outline; styleColor: "white"
            }
            TestText {
                text: s.text; style: Text.Sunken; styleColor: "gray"
            }
            TestText {
                text: s.text; style: Text.Raised; styleColor: "yellow"
            }
            TestText {
                text: s.text; font.letterSpacing: 2
            }
        }
        TestText {
            text: s.text; font.underline: true; font.letterSpacing: 2; font.capitalization: "AllUppercase"; color: "blue"
        }
        TestText {
            text: s.text; font.overline: true; font.wordSpacing: 25; font.capitalization: "Capitalize"; color: "green"
        }
        Row{
            height: childrenRect.height
            spacing: 4
            TestText {
                text: s.text; elide: Text.ElideLeft; width: 200
            }
            TestText {
                text: s.text; elide: Text.ElideMiddle; width: 200
            }
            TestText {
                text: s.text; elide: Text.ElideRight; width: 200
            }
        }
        Row{
            height: childrenRect.height
            spacing: 4
            TestText{
                text: s.text; elide: Text.ElideLeft; width: 200; wrapMode: Text.WordWrap
            }
            TestText {
                text: s.text; elide: Text.ElideMiddle; width: 200; wrapMode: Text.WordWrap
            }
            TestText {
                text: s.text; elide: Text.ElideRight; width: 200; wrapMode: Text.WordWrap
            }
        }
        Row{
            height: childrenRect.height
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
