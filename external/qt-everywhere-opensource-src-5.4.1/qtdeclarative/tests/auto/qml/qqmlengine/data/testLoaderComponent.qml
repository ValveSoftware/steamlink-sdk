import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testLoaderComponent.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')

        loader.source = 'VMEExtendVMEComponent.qml'
    }

    Loader {
        id: loader

        property bool previouslyLoaded: false
        onLoaded: {
            if (!previouslyLoaded) {
                componentCache.trim()
                if (!componentCache.isTypeLoaded('testLoaderComponent.qml')) return reportError('Test component not loaded 2')
                if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already unloaded')
                if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already unloaded')

                if (!item) return reportError('Invalid item')
                if (item.foo != 'bar') return reportError('Invalid item 2')
                if (item.bar != 'baz') return reportError('Invalid item 3')

                loader.source = ''
                componentCache.trim()
                if (!componentCache.isTypeLoaded('testLoaderComponent.qml')) return reportError('Test component not loaded 3')
                if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not unloaded')
                if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')
                if (item) return reportError('Item not invalidated')

                previouslyLoaded = true
                loader.source = 'VMEExtendVMEComponent.qml'
            } else {
                componentCache.trim()
                if (!componentCache.isTypeLoaded('testLoaderComponent.qml')) return reportError('Test component not loaded 4')
                if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not reloaded')
                if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not reloaded')

                if (!item) return reportError('Invalid item 4')
                if (item.foo != 'bar') return reportError('Invalid item 5')
                if (item.bar != 'baz') return reportError('Invalid item 6')

                loader.source = ''
                componentCache.trim()
                if (!componentCache.isTypeLoaded('testLoaderComponent.qml')) return reportError('Test component not loaded 5')
                if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not unloaded 2')
                if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded 2')
                if (item) return reportError('Item not invalidated 2')

                success = true
            }
        }
    }
}
