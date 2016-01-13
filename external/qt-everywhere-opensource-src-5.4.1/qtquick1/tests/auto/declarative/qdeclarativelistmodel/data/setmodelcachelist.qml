import QtQuick 1.0

ListModel {
    id: model
    property bool ok : false

    Component.onCompleted: {
        model.append({"attrs": []})
        model.get(0)
        model.set(0, {"attrs": [{'abc': 123, 'def': 456}] } )
        ok = ( model.get(0).attrs.get(0).abc == 123
            && model.get(0).attrs.get(0).def == 456 )

        model.set(0, {"attrs": [{'abc': 789, 'def': 101}] } )
        ok = ( model.get(0).attrs.get(0).abc == 789
            && model.get(0).attrs.get(0).def == 101 )

    }
}

