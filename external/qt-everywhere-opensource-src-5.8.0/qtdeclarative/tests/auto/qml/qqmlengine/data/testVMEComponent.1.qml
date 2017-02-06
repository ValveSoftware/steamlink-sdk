import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEComponent.1.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')

        var comp = Qt.createComponent('VMEComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEComponent.1.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not loaded')

        var obj = comp.createObject()
        if (!obj) return reportError('Invalid object')
        if (obj.foo != 'bar') return reportError('Invalid object 2')

        obj.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEComponent.1.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded')

        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEComponent.1.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')

        success = true
    }
}
