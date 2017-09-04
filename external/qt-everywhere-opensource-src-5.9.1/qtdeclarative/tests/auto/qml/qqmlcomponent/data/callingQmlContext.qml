import QtQml 2.0
import qqmlcomponenttest 1.0
QtObject {
    property Component factory
    property QtObject incubatedObject

    Component.onCompleted: {
        var incubatorState = factory.incubateObject(null, { value: 42 })
        incubatorState.onStatusChanged = function(status) {
            incubatedObject = incubatorState.object
        }
    }
}
