import QtQuick 2.0

Item {
    width: 200
    height: 442

    ListModel {
        id: listModel
        ListElement {
            name: "h1"
            txt: "Header 1"
            header: true
            collapsed: true
        }
        ListElement {
            name: "h2"
            txt: "Header 2"
            header: true
            collapsed: true
        }
        ListElement {
            name: "h3"
            txt: "Header 3"
            header: true
            collapsed: true
        }

        function indexFromName(name) {
            for (var i = 0; i < count; i++)
                if (get(i).name === name)
                    return i

            console.warn("Did not find index for name " + name)
            return -1
        }
    }

    function populateModel(prefix, index, n) {
        for (var k = 1; k <= n; k++) {
            var name = prefix + k
            var data = {
                "collapsed": false,
                "name": name,
                "txt": name,
                "header": false
            }
            listModel.insert(index + k, data)
        }
    }

    function h2(open) {
        var i = listModel.indexFromName("h2")
        if (listModel.get(i).collapsed === !open)
            return

        listModel.setProperty(i, "collapsed", !open)

        var n = 15
        if (open) {
            h3(false)
            populateModel("c2_", listModel.indexFromName("h2"), n)
        } else {
            listModel.remove(i + 1, n)
        }

    }

    function h3(open) {
        var i = listModel.indexFromName("h3")
        if (listModel.get(i).collapsed === !open)
            return

        listModel.setProperty(i, "collapsed", !open)

        var n = 6
        if (open) {
            h2(false)
            populateModel("c3_", listModel.indexFromName("h3"), n)
        } else {
            listModel.remove(i + 1, n)
        }
    }

    ListView {
        id: listView
        width: parent.width
        height: parent.height
        cacheBuffer: 0
        model: listModel

        property bool transitionsDone: false
        property int runningTransitions: 0
        onRunningTransitionsChanged: {
            if (runningTransitions === 0)
                transitionsDone = true
        }

        displaced: Transition {
            id: dispTrans
            SequentialAnimation {
                ScriptAction {
                    script: listView.runningTransitions++
                }
                NumberAnimation {
                    property: "y";
                    duration: 250
                }
                ScriptAction {
                    script: listView.runningTransitions--
                }
            }
        }

        delegate: Rectangle {
            id: rect
            color: header ? "yellow" : "cyan"
            border.color: "black"
            height: 50
            width: parent.width

            Text {
                anchors.centerIn: parent
                font.pixelSize: 20
                text: txt
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listView.currentIndex = index
                    var i = listModel.indexFromName("h3")
                    if (i === -1)
                        return;
                    var isCollapsed = listModel.get(i).collapsed
                    if (name === "h2")
                        h2(isCollapsed)
                    else if (name === "h3")
                        h3(isCollapsed)
                }
            }
        }
    }
}

