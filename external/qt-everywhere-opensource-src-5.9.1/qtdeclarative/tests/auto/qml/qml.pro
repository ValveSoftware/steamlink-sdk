TEMPLATE = subdirs

METATYPETESTS += \
    qqmlmetatype

PUBLICTESTS += \
    parserstress \
    qjsvalueiterator \
    qjsonbinding \
    qqmlfile \

!boot2qt {
PUBLICTESTS += \
    qmlmin \
    qqmlcomponent \
    qqmlconsole \
    qqmlengine \
    qqmlerror \
    qqmlincubator \
    qqmlinfo \
    qqmllistreference \
    qqmllocale \
    qqmlmetaobject \
    qqmlmoduleplugin \
    qqmlnotifier \
    qqmlqt \
    qqmlxmlhttprequest \
    qtqmlmodules \
    qquickfolderlistmodel \
    qqmlapplicationengine \
    qqmlsettings \
    qqmlstatemachine \
    qmldiskcache
}

PRIVATETESTS += \
    qqmlcpputils \
    qqmldirparser \
    v4misc \
    qmlcachegen

!boot2qt {
PRIVATETESTS += \
    animation \
    qqmlecmascript \
    qqmlcontext \
    qqmlexpression \
    qqmlglobal \
    qqmllanguage \
    qqmlopenmetaobject \
    qqmlproperty \
    qqmlpropertycache \
    qqmlpropertymap \
    qqmlsqldatabase \
    qqmlvaluetypes \
    qqmlvaluetypeproviders \
    qqmlbinding \
    qqmlchangeset \
    qqmlconnections \
    qqmllistcompositor \
    qqmllistmodel \
    qqmllistmodelworkerscript \
    qqmlitemmodels \
    qqmltypeloader \
    qqmlparser \
    qquickworkerscript \
    qrcqml \
    qqmltimer \
    qqmlinstantiator \
    qqmlenginecleanup \
    qqmltranslation \
    qqmlimport \
    qqmlobjectmodel \
    qv4mm \
    ecmascripttests
}

qtHaveModule(widgets) {
    PUBLICTESTS += \
        qjsengine \
        qjsvalue
}

SUBDIRS += $$PUBLICTESTS
SUBDIRS += $$METATYPETESTS
qtConfig(process):!boot2qt {
    !contains(QT_CONFIG, no-qml-debug): SUBDIRS += debugger
    SUBDIRS += qmllint qmlplugindump
}

qtConfig(library) {
    SUBDIRS += qqmlextensionplugin
}

qtConfig(private_tests): \
    SUBDIRS += $$PRIVATETESTS

qtNomakeTools( \
    qmlplugindump \
)
