import QtQuick 2.0

PathView {
    width: 240
    height: 200

    path: myPath

    delegate: Text { text: value }
    model: 10

    Path {
        id: myPath
        startX: 120; startY: 100
        PathQuad { x: 120; y: 25; controlX: 260; controlY: 75 }
        PathQuad { x: 120; y: 100; controlX: -20; controlY: 75 }
    }

    function removePath() {
        path = null
    }

    function setPath() {
        path = myPath
    }
}
