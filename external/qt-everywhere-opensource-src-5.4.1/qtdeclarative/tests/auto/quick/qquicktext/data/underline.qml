import QtQuick 2.0

Item {
    width: 200
    height: 200

    Text {
        id: myText
        objectName: "myText"
        width: 200
        wrapMode: Text.WordWrap
        font.underline: true
        text: "Testing that maximumLines, visibleLines, and totalLines works properly in the autotests. The quick brown fox jumped over the lazy anything with the letter 'g'."
    }
}
