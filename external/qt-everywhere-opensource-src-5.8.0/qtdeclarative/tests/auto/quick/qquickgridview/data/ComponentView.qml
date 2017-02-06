import QtQuick 2.0

GridView {
    id: view

    property string title

    width: 100; height: 100;

    model: 1
    delegate: Text { objectName: "listItem"; text: view.title }
    header: Text { objectName: "header"; text: view.title }
    footer: Text { objectName: "footer"; text: view.title }
}
