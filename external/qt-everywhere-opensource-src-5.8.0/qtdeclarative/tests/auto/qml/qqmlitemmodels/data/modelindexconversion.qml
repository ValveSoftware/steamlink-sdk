import Test 1.0

ItemModelsTest {

    onModelChanged: {
        modelIndex = createPersistentModelIndex(model.index(0, 0))
        persistentModelIndex = model.index(1, 1)
    }
}
