import QtQuick 2.0

Rectangle {
    property bool sectionsInvalidOnCompletion
    width: 240
    height: 320
    color: "#ffffff"
    resources: [
        Component {
            id: myDelegate
            Item {
                id: wrapper

                function validateInitialSections() {
                    var invalid = false
                    if (index == 0) {
                        invalid |= wrapper.ListView.previousSection != ""
                    }
                    if (index == model.count - 1) {
                        invalid |= wrapper.ListView.nextSection != ""
                    }
                    if (index % 5 == 0 && index > 0) {
                        invalid |= wrapper.ListView.previousSection != Number(wrapper.ListView.section) - 1
                    } else if ((index + 1) % 5 == 0 && index < model.count - 1) {
                        invalid |= wrapper.ListView.nextSection != Number(wrapper.ListView.section) + 1
                    } else if (index > 0 && index < model.count - 1) {
                        invalid |= wrapper.ListView.previousSection != wrapper.ListView.section
                        invalid |= wrapper.ListView.nextSection != wrapper.ListView.section
                    }
                    sectionsInvalidOnCompletion |= invalid
                }

                objectName: "wrapper"
                height: ListView.previousSection != ListView.section ? 40 : 20;
                width: 240
                Rectangle {
                    y: wrapper.ListView.previousSection != wrapper.ListView.section ? 20 : 0
                    height: 20
                    width: parent.width
                    color: wrapper.ListView.isCurrentItem ? "lightsteelblue" : "white"
                    Text {
                        text: index
                    }
                    Text {
                        x: 30
                        id: textName
                        objectName: "textName"
                        text: name
                    }
                    Text {
                        x: 100
                        id: textNumber
                        objectName: "textNumber"
                        text: number
                    }
                    Text {
                        objectName: "nextSection"
                        x: 150
                        text: wrapper.ListView.nextSection
                    }
                    Text {
                        x: 200
                        text: wrapper.y
                    }
                }
                Rectangle {
                    color: "#99bb99"
                    height: wrapper.ListView.previousSection != wrapper.ListView.section ? 20 : 0
                    width: parent.width
                    visible: wrapper.ListView.previousSection != wrapper.ListView.section ? true : false
                    Text { text: wrapper.ListView.section }
                }
                ListView.onPreviousSectionChanged: validateInitialSections()
                ListView.onNextSectionChanged: validateInitialSections()
                ListView.onSectionChanged: validateInitialSections()
                Component.onCompleted: validateInitialSections()
            }
        }
    ]
    ListView {
        id: list
        objectName: "list"
        width: 240
        height: 320
        model: testModel
        delegate: myDelegate
        section.property: "number"
    }
}
