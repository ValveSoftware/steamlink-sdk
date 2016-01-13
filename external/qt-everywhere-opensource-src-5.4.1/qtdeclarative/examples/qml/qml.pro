TEMPLATE = subdirs

qtHaveModule(quick): SUBDIRS += \
    networkaccessmanagerfactory \
    qmlextensionplugins \
    xmlhttprequest

SUBDIRS += \
          referenceexamples \
          shell

EXAMPLE_FILES = \
    dynamicscene \
    qml-i18n \
    locale
