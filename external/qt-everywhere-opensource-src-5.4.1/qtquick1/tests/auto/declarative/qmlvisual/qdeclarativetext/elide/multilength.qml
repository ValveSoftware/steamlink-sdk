import QtQuick 1.0
import "../../shared" 1.0

Rectangle {
    width: 500
    height: 50
    color: "lightBlue"
    Rectangle {
        width: myText.width
        height: myText.height
        color: "white"
        anchors.centerIn: parent
        TestText {
            id: myText
            NumberAnimation on width { from: 500; to: 0; loops: Animation.Infinite; duration: 5000 }
            elide: "ElideRight"
            text: "Brevity is the soul of wit, and tediousness the limbs and outward flourishes.\x9CBrevity is a great charm of eloquence.\x9CBe concise!\x9CSHHHHHHHHHHHHHHHHHHHHHHHHHHHH"
        }
    }
}
