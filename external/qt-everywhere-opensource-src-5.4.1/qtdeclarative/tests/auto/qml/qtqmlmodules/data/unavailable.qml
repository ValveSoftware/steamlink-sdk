import QtQml 2.0

QtObject {
    id: root
    property bool success: false;
    Component.onCompleted: {
        var strings = [
            "QtObject{}",
            "Binding{}",
            "Connections{}",
            "Timer{}",
            "Component{QtObject{}}",
            "ListModel{ListElement{}}",
            "ObjectModel{QtObject{}}",
            "import QtQml 2.0 Item{}",
            "import QtQml 2.0 ListModel{}",
            "import QtQml 2.0 ObjectModel{}"
        ];
        var idx;
        for (idx in strings) {
            var errored = false;
            var item;
            try {
                item = Qt.createQmlObject(strings[idx], root);
            } catch (err) {
                errored = true;
            }
            if (!errored) {
                console.log("It worked? ", item);
                return;
            }
        }
        root.success = true;
    }
}
