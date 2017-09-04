import QtQuick 2.4

PathView {
    width: 200
    height: 600

    pathItemCount: 7

    model: ListModel {
        ListElement { color: "salmon" }
        ListElement { color: "seagreen" }
        ListElement { color: "navy" }
        ListElement { color: "goldenrod" }
    }
    path: Path {
        startX: width / 2; startY: -100
        PathAttribute { name: "BEGIN" }

        PathLine { relativeX: 0; y: height / 2 }
        PathAttribute { name: "BEGIN" }

        PathLine { relativeX: 0; y: height + 100 }
        PathAttribute { name: "BEGIN" }
    }
    delegate: Rectangle {
        width: 200
        height: 200
        color: model.color
        opacity: PathView.transparency
    }

    Component {
        id: attributeComponent
        PathAttribute {}
    }

    function addAttribute(name, values) {
        var valueIndex = 0
        var elements = []
        for (var i = 0; i < path.pathElements.length; ++i) {
            elements.push(path.pathElements[i])

            if (path.pathElements[i].name === "BEGIN") {
                if (values[valueIndex] !== undefined) {
                    var attribute = attributeComponent.createObject(this, { "name": name, "value": values[valueIndex] })
                    elements.push(attribute)
                }
                ++valueIndex
            }
        }

        console.log("??")
        path.pathElements = elements
        console.log("!!")
    }

    Component.onCompleted: addAttribute("transparency", [0, 1, 0])
}
