.pragma library

function loadComponent() {
    var component = Qt.createComponent("createComponentData.qml");
    return component.status;
}

function createComponent() {
    var component = Qt.createComponent("createComponentData.qml");
    return component.createObject(null);
}
