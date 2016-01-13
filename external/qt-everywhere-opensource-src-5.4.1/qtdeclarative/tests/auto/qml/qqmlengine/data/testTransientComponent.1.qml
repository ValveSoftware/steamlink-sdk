import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTransientComponent.1.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component already loaded')

        var comp = Qt.createComponent('VMEExtendVMEComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTransientComponent.1.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component not loaded')

        var obj = comp.createObject()
        if (!obj) return
        if (obj.foo != 'bar') return reportError('Invalid object')
        if (obj.bar != 'baz') return reportError('Invalid object 2')

        obj.destroy()
        if (!componentCache.isTypeLoaded('testTransientComponent.1.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component already unloaded')

        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTransientComponent.1.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component not unloaded')

        success = true
    }
}
