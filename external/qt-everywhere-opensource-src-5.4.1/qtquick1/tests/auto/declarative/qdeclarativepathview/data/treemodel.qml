import QtQuick 1.0

PathView {
    width: 320
    height: 240
    function setRoot(index) {
        vdm.rootIndex = vdm.modelIndex(index);
    }
    model: VisualDataModel {
        id: vdm
        model: myModel
        delegate: Text { objectName: "wrapper"; text: display }
    }

    path: Path {
        startX: 0; startY: 120
        PathLine { x: 320; y: 120 }
    }
}
