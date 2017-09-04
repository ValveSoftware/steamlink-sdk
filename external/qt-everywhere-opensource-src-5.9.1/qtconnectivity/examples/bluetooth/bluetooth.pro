TEMPLATE = subdirs

SUBDIRS += heartrate-server

qtHaveModule(widgets) {
    SUBDIRS += btchat \
               btscanner \
               btfiletransfer
}

qtHaveModule(quick): SUBDIRS += scanner \
                                picturetransfer \
                                pingpong \
                                lowenergyscanner \
                                heartrate-game \
                                chat
