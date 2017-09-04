import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEPropertyVMEComponent.2.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEPropertyVMEComponent.qml')) return reportError('Property component already loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')

        var comp = Qt.createComponent('VMEPropertyVMEComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEPropertyVMEComponent.2.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('VMEPropertyVMEComponent.qml')) return reportError('Property component not loaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not loaded')

        var obj = comp.createObject()
        if (!obj) return reportError('Invalid object')
        if (obj.foo != 'bar') return reportError('Invalid object 2')
        if (obj.p == undefined) return reportError('Invalid object 3')
        if (obj.p.foo != 'bar') return reportError('Invalid object 4')

        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEPropertyVMEComponent.2.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('VMEPropertyVMEComponent.qml')) return reportError('Property component already unloaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded')
        if (!obj) return reportError('Invalid object 5')
        if (obj.foo != 'bar') return reportError('Invalid object 6')
        if (obj.p == undefined) return reportError('Invalid object 7')
        if (obj.p.foo != 'bar') return reportError('Invalid object 8')

        obj.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEPropertyVMEComponent.2.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('VMEPropertyVMEComponent.qml')) return reportError('Property component not unloaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')

        success = true
    }
}
