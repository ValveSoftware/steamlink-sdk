import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testIncubatedComponent.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')

        var c = Qt.createComponent('VMEExtendVMEComponent.qml')
        var i = c.incubateObject(null, {}, Qt.Asynchronous)

        componentCache.trim()
        if (!componentCache.isTypeLoaded('testIncubatedComponent.qml')) return reportError('Test component unloaded 1')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not loaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not loaded')

        c.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testIncubatedComponent.qml')) return reportError('Test component unloaded 2')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already unloaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded')

        i.onStatusChanged = function(status) {
            if (status != Component.Ready) return;
            if (i.object == null) return reportError('Extend component not created')
            if (i.object.foo != 'bar') return reportError('Invalid object')
            if (i.object.bar != 'baz') return reportError('Invalid object 2')

            componentCache.trim()
            if (!componentCache.isTypeLoaded('testIncubatedComponent.qml')) return reportError('Test component unloaded 3')
            if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already unloaded 2')
            if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded 2')

            i.object.destroy()
            componentCache.trim()
            if (!componentCache.isTypeLoaded('testIncubatedComponent.qml')) return reportError('Test component unloaded 4')
            if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not unloaded')
            if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')

            success = true
        }

        componentCache.beginIncubation()
        componentCache.waitForIncubation();
    }
}
