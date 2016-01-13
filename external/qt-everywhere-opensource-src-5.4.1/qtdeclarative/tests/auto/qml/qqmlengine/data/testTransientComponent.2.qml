import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTransientComponent.2.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component already loaded')

        var comp = Qt.createComponent('VMEExtendVMEComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTransientComponent.2.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component not loaded')

        var obj = comp.createObject()
        if (!obj) return
        if (obj.foo != 'bar') return reportError('Invalid object')
        if (obj.bar != 'baz') return reportError('Invalid object 2')

        comp.destroy()
        if (!componentCache.isTypeLoaded('testTransientComponent.2.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component already unloaded')
        if (obj.foo != 'bar') return reportError('Invalid object 3')
        if (obj.bar != 'baz') return reportError('Invalid object 4')

        obj.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTransientComponent.2.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Transient component not unloaded')

        success = true
    }
}
