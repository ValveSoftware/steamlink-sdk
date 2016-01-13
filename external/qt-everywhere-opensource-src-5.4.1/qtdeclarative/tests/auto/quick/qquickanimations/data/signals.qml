import QtQuick 2.0


Item {
    id: wrapper
    width: 400; height: 400

    function start() { animation.start() }
    function stop() { animation.stop() }
    property alias alwaysRunToEnd: animation.alwaysRunToEnd

    property int startedCount: 0
    property int stoppedCount: 0

    Rectangle {
        id: greenRect
        width: 50; height: 50
        color: "green"

        NumberAnimation on x {
            id: animation
            from: 0
            to: 100
            duration: 200

            onStarted: ++startedCount
            onStopped: ++stoppedCount
        }
    }
}
