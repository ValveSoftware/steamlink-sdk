TEMPLATE = subdirs
qtHaveModule(widgets) {
    SUBDIRS += btchat \
               btscanner \
               btfiletransfer
}

qtHaveModule(quick): SUBDIRS += scanner \
                                picturetransfer \
                                pingpong \
                                lowenergyscanner \
                                heartlistener \
                                chat
