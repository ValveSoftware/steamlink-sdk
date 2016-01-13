import QtQuick 1.0
import "../../shared" 1.0

/*Tests both the alignments of multiline text, and that
  it can deal with changing them properly
*/
Item{
    width: 100
    height: 80
    property int stage: 0
    onStageChanged: if(stage == 6) Qt.quit();
    TestText{
        text: "I am the very model of a modern major general."
        anchors.fill: parent;
        wrapMode: Text.WordWrap
        horizontalAlignment: (stage<1 ? Text.AlignRight : (stage<3 ? Text.AlignHCenter : Text.AlignLeft))
        verticalAlignment: (stage<2 ? Text.AlignBottom : (stage<4 ? Text.AlignVCenter : Text.AlignTop))
    }
    Timer{
        interval: 160
        running: true
        repeat: true
        onTriggered: stage += 1
    }
}
