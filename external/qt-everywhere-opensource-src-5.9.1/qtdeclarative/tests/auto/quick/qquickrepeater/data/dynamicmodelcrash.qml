import QtQuick 2.0

Item {
    ListModel {
        id: lm;
    }

    Component.onCompleted: {
        lm.append({ subModel: [ {d:0} ] });
        rep.model = lm.get(0).subModel;
        rep.model;
        lm.remove(0);
        rep.model;
    }

    Repeater {
        objectName: "rep"
        id: rep
    }
}
