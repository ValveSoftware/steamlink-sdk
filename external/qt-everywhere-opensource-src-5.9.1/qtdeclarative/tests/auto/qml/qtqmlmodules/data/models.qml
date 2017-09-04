import QtQml 2.0
import QtQml.Models 2.1

QtObject {
    property bool success: {
        prop1 != undefined &&
        prop2 != undefined &&
        prop3 != undefined &&
        prop4 != undefined
    }
    property DelegateModelGroup prop1: DelegateModelGroup{}
    property DelegateModel prop2: DelegateModel{}
    property ObjectModel prop3: ObjectModel{}
    property ListModel prop4: ListModel{ListElement{dummy: true}}
}
