import Test 1.0

ItemModelsTest {
    property int count
    property bool contains: false
    property var itemSelectionBinding: itemSelection
    property var itemSelectionRead

    function range(top, bottom, left, right, parent) {
        if (parent === undefined)
            parent = invalidModelIndex()
        var topLeft = model.index(top, left, parent)
        var bottomRight = model.index(bottom, right, parent)
        return createItemSelectionRange(topLeft, bottomRight)
    }

    onModelChanged: {
        itemSelection = []
        itemSelection.push(range(0, 0, 0, 5))
        itemSelection.push(range(0, 5, 0, 0))
        for (var i = 0; i < 3; i++)
            itemSelection.splice(i, 0, range(i, i + 1, i + 2, i + 3))

        itemSelectionRead = itemSelection

        count = itemSelection.length
        contains = itemSelection.some(function (range, idx) { return range.contains(model.index(0, 0)) })
    }
}
