import QtQuick 1.0
import "../../shared" 1.0

/* The bug was that if text was set to "" or the size didn't increase, the text didn't repaint
   ended up only repainting for 1, 10, 11, 12.
   Test passes if it goes from "" to 13 back to "" with all numbers being painted (and the text disappearing at 0)
 */

Item{
    width: 80
    height: 80
    property int val: 0
    Text{
        id: txt;
        text: val == 0 ? "" : val
    }
    Timer{
        interval: 100
        running: true
        repeat: true;
        onTriggered: {val = (val + 1) % 14;}
    }
}
