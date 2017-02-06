import QtQuick 2.0

Rectangle {
    width: 100
    height: 100
    color: "blue"

    RotationAnimation on rotation {
        duration: 3600
        loops: Animation.Infinite
        from: 0
        to: 360
    }

    Timer {
        interval: 300
        repeat: true
        running: true
        property int prevHit: -1
        property int prevRotation: -1
        onTriggered: {
            var date = new Date;
            var millis = date.getMilliseconds()

            if (prevHit < 0) {
                prevHit = millis;
                prevRotation = parent.rotation
                return;
            }

            var milliDelta = millis - prevHit;
            if (milliDelta < 0)
                milliDelta += 1000;
            console.log(milliDelta, "milliseconds ");
            prevHit = millis;

            var delta = parent.rotation - prevRotation;
            if (delta < 0)
                delta += 360
            prevRotation = parent.rotation
            console.log(delta, "degrees ");
        }
    }
}
