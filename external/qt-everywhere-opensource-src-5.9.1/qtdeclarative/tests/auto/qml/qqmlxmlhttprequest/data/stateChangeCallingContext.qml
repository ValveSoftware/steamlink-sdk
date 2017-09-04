import QtQuick 2.0

Item {
    id: root

    property int whichCount: 0
    property bool success: false
    property string serverBaseUrl;

    SequentialAnimation {
        id: anim
        PauseAnimation { duration: 1000 } // delay mode is 500 msec.
        ScriptAction { script: loadcomponent(0) }
        PauseAnimation { duration: 525 } // delay mode is 500 msec.
        ScriptAction { script: loadcomponent(1) }
        PauseAnimation { duration: 525 } // delay mode is 500 msec.
        ScriptAction { script: if (whichCount == 2) root.success = true; else console.log("failed to load testlist"); }
    }

    Component.onCompleted: {
        updateList();
        anim.start();
    }

    function updateList() {
        var xhr = new XMLHttpRequest();
        xhr.open("GET",serverBaseUrl + "/testlist"); // list of components
        xhr.onreadystatechange = function () {
            if (xhr.readyState == XMLHttpRequest.DONE) {
                var components = xhr.responseText.split('\n');
                var i;
                for (i=0; i<components.length; i++) {
                    var pair = components[i].split(";")
                    if (pair.length == 2) {
                        // Trim any unwanted whitespace
                        var name = pair[0].replace(/^\s+|\s+$/g, "")
                        var url = pair[1].replace(/^\s+|\s+$/g, "")
                        componentlist.append({"Name" : name, "url" : url})
                    }
                }
            }
        }
        xhr.send()
    }

    function loadcomponent(which) {
        if (componentlist.count > which) {
            loader.source = componentlist.get(which).url;
            whichCount += 1;
        }
    }

    Loader {
        id: loader
        signal finished

        anchors.fill: parent
        onStatusChanged: {
            if (status == Loader.Error) { finished(); next(); }
        }
    }

    ListModel { id: componentlist }
}
