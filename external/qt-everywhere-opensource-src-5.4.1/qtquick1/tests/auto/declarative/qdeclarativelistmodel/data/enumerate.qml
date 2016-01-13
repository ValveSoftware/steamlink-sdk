import QtQuick 1.0

Item {
    property string result

    ListModel {
        id: model

        ListElement {
            val1: 1
            val2: 2
            val3: "str"
            val4: false
            val5: true
        }
    }

    Component.onCompleted: {
        var element = model.get(0);

        for (var i in element)
            result += i+"="+element[i]+(element[i] ? "Y" : "N")+":";
    }
}
