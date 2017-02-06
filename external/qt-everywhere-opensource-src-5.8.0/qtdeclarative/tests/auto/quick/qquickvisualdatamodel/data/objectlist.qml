import QtQuick 2.0

ListView {
    width: 100
    height: 100
    anchors.fill: parent
    model: myModel
    delegate: Component {
        Rectangle {
            height: 25
            width: 100
            color: model.modelData.color
            Text { objectName: "name"; text: name; property string modelName: model.name; function getText() { return name }  }
            Text { objectName: "section"; text: parent.ListView.section }
        }
    }

    function changeSectionProperty() {
        section.property = "object.subName"
    }
    section.property: "name"
    section.criteria: ViewSection.FullString
}
