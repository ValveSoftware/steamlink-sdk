import Test 1.0

ItemModelsTest {
    property int count
    property var modelIndexListCopy
    property var modelIndexListRead
    property var modelIndexListBinding: modelIndexList
    property bool varPropIsArray
    property bool varIsArray
    property bool propIsArray

    onModelChanged: {
        var jsModelIndexList = []
        for (var i = 0; i < 3; i++)
            jsModelIndexList.push(model.index(2 + i, 2 + i))
        jsModelIndexList.push("Hi Bronsky!")
        modelIndex = jsModelIndexList[0]

        count = modelIndexList.length
        propIsArray = modelIndexList instanceof Array
        modelIndexList = jsModelIndexList
        modelIndexListRead = modelIndexList

        modelIndexListCopy = someModelIndexList()
        varPropIsArray = modelIndexListCopy instanceof Array

        jsModelIndexList = someModelIndexList()
        varIsArray = jsModelIndexList instanceof Array
    }
}
