import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEAggregateVMEComponent.2.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Aggregate component already loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')

        var comp = Qt.createComponent('VMEAggregateVMEComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEAggregateVMEComponent.2.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('VMEAggregateVMEComponent.qml')) return reportError('Aggregate component not loaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not loaded')

        var obj = comp.createObject()
        if (!obj) return reportError('Invalid object')
        if (obj.foo != 'bar') return reportError('Invalid object 2')
        if (obj.children[0].foo != 'bar') return reportError('Invalid object 3')

        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEAggregateVMEComponent.2.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('VMEAggregateVMEComponent.qml')) return reportError('Aggregate component already unloaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded')
        if (!obj) return reportError('Invalid object r4')
        if (obj.foo != 'bar') return reportError('Invalid object 5')
        if (obj.children[0].foo != 'bar') return reportError('Invalid object 6')

        obj.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testVMEAggregateVMEComponent.2.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('VMEAggregateVMEComponent.qml')) return reportError('Aggregate component not unloaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')

        success = true
    }
}
