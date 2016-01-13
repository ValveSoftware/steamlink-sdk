import QtQuick 2.0

VisualDataModel {
    id: vm

    property var inserted
    property var removed

    Component.onCompleted: {
        vm.inserted = []
        vm.removed = []
        vi.inserted = []
        vi.removed = []
        si.inserted = []
        si.removed = []
    }

    function verify(changes, indexes, counts, moveIds) {
        if (changes.length != indexes.length
                || changes.length != counts.length
                || changes.length != moveIds.length) {
            console.log("invalid length", changes.length, indexes.length, counts.length, moveIds.length)
            return false
        }

        var valid = true;
        for (var i = 0; i < changes.length; ++i) {
            if (changes[i].index != indexes[i]) {
                console.log(i, "incorrect index. actual:", changes[i].index, "expected:", indexes[i])
                valid = false;
            }
            if (changes[i].count != counts[i]) {
                console.log(i, "incorrect count. actual:", changes[i].count, "expected:", counts[i])
                valid = false;
            }
            if (changes[i].moveId != moveIds[i]) {
                console.log(i, "incorrect moveId. actual:", changes[i].moveId, "expected:", moveIds[i])
                valid = false;
            }
        }
        return valid
    }

    groups: [
        VisualDataGroup {
            id: vi;

            property var inserted
            property var removed

            name: "visible"
            includeByDefault: true

            onChanged: {
                vi.inserted = inserted
                vi.removed = removed
            }
        },
        VisualDataGroup {
            id: si;

            property var inserted
            property var removed

            name: "selected"
            onChanged: {
                si.inserted = inserted
                si.removed = removed
            }
        }
    ]

    model: ListModel {
        id: listModel
        ListElement { number: "one" }
        ListElement { number: "two" }
        ListElement { number: "three" }
        ListElement { number: "four" }
    }

    delegate: Item {}

    items.onChanged: {
        vm.inserted = inserted
        vm.removed = removed
    }
}
