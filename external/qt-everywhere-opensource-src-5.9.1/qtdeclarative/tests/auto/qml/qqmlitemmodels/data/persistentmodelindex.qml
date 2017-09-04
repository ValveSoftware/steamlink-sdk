import Test 1.0

ItemModelsTest {
    property bool isValid: persistentModelIndex.valid
    property int row: persistentModelIndex.row
    property int column: persistentModelIndex.column
    property var parent: persistentModelIndex.parent
    property var model: persistentModelIndex.model
    property var internalId: persistentModelIndex.internalId

    property var pmi

    onSignalWithPersistentModelIndex: {
        isValid = index.valid
        row = index.row
        column = index.column
        parent = index.parent
        model = index.model
        internalId = index.internalId

        pmi = createPersistentModelIndex(model.index(0, 0))
    }
}
