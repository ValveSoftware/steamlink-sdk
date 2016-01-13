import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTopLevelComponent.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('TopLevelComponent.qml')) return reportError('Top-level component already loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')

        var comp = Qt.createComponent('TopLevelComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTopLevelComponent.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('TopLevelComponent.qml')) return reportError('Top-level component not loaded')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not loaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not loaded')

        var internalComp = comp.createObject()
        if (!internalComp) return reportError('Invalid component')

        var obj = internalComp.createObject()
        if (!obj) return reportError('Invalid object')
        if (obj.foo != 'bar') return reportError('Invalid object 2')
        if (obj.bar != 'baz') return reportError('Invalid object 3')

        internalComp.destroy()
        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTopLevelComponent.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('TopLevelComponent.qml')) return reportError('Top-level component already unloaded')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already unloaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded')
        if (!obj) return reportError('Invalid object 4')
        if (obj.foo != 'bar') return reportError('Invalid object 5')
        if (obj.bar != 'baz') return reportError('Invalid object 6')

        obj.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testTopLevelComponent.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('TopLevelComponent.qml')) return reportError('Top-level component not unloaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not unloaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')

        success = true
    }
}
