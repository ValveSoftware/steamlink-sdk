TEMPLATE = aux

exampleassets.files += \
    chatclient.js \
    package.json
exampleassets.path = $$[QT_INSTALL_EXAMPLES]/webchannel/nodejs
include(../exampleassets.pri)
