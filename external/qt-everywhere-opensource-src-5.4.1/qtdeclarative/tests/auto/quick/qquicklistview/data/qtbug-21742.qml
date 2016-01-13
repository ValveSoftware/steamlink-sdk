import QtQuick 2.0

Rectangle {
    height: 200
    width: 200
    property int count: menuView.count

    Component.onCompleted: { setModel(); }

    function setModel()
    {
        menuModel.append({"enabledItem" : true});
        menuView.currentIndex = 0;
    }

    ListModel {
        id: menuModel
    }

    ListView {
        id: menuView
        anchors.fill: parent
        model: menuModel
        delegate: mything
    }

    Component {
        id: mything
        Rectangle {
            height: 50
            width: 200
            color: index == menuView.currentIndex ? "green" : "blue"
        }
    }

}