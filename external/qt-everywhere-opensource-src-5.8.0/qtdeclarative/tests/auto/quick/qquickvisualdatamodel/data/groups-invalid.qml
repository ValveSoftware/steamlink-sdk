import QtQuick 2.0

VisualDataModel {
    id: visualModel

    objectName: "visualModel"

    groups: [
        VisualDataGroup { id: visibleItems; objectName: "visibleItems"; name: "visible"; includeByDefault: true },
        VisualDataGroup { id: selectedItems; objectName: "selectedItems"; name: "selected" },
        VisualDataGroup { id: unnamed; objectName: "unnamed" },
        VisualDataGroup { id: capitalised; objectName: "capitalised"; name: "Capitalised" }
    ]
}
