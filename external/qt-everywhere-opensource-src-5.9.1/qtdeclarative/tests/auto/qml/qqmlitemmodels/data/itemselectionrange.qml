import Test 1.0

ItemModelsTest {
    property var itemSelectionRange: createItemSelectionRange(invalidModelIndex(), invalidModelIndex())
    property int top: itemSelectionRange.top
    property int left: itemSelectionRange.left
    property int bottom: itemSelectionRange.bottom
    property int right: itemSelectionRange.right
    property int width: itemSelectionRange.width
    property int height: itemSelectionRange.height
    property bool isValid: itemSelectionRange.valid
    property bool isEmpty: itemSelectionRange.empty
    property var isrModel: itemSelectionRange.model
    property bool contains1: false
    property bool contains2: false
    property bool intersects: false
    property var intersected

    onModelChanged: {
        if (model) {
            var parentIndex = model.index(0, 0)
            var index1 = model.index(3, 0, parentIndex)
            var index2 = model.index(5, 6, parentIndex)
            itemSelectionRange = createItemSelectionRange(index1, index2)

            contains1 = itemSelectionRange.contains(index1)
            contains2 = itemSelectionRange.contains(4, 3, parentIndex)
            intersects = itemSelectionRange.intersects(createItemSelectionRange(parentIndex, parentIndex))
            intersected = itemSelectionRange.intersected(createItemSelectionRange(parentIndex, parentIndex))
        }
    }
}
