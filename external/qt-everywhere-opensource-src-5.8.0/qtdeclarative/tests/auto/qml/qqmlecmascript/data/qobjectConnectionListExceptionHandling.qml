import QtQuick 2.0

Item {
    id: root
    property int first: 5
    property bool test: false

    Item {
        id: exceptional
        function exceptionalFunction() {
            var obj = undefined;
            var prop = undefined;
            return obj[prop];
        }
    }

    Component.onCompleted: {
        root["firstChanged"].connect(exceptional.exceptionalFunction);
        root["firstChanged"].connect(exceptional.exceptionalFunction);
        root["firstChanged"].connect(exceptional.exceptionalFunction);
        first = 6;
        test = true;
    }
}
