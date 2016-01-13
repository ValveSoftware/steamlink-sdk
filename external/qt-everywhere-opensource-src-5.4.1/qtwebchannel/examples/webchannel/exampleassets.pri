# This adds the qwebchannel js library to an example, creating a self-contained bundle
QTDIR_build {
    # Build from within Qt. Copy and install the reference lib.
    jslib = $$dirname(_QMAKE_CONF_)/src/webchannel/qwebchannel.js
    copyfiles = $$jslib
} else {
    # This is what an actual 3rd party project would do.
    jslib = qwebchannel.js
}

# This installs all assets including qwebchannel.js, regardless of the source.
exampleassets.files += $$jslib
INSTALLS += exampleassets

# This code ensures that all assets are present in the build directory.

!equals(_PRO_FILE_PWD_, $$OUT_PWD) {
    # Shadow build, copy all example assets.
    copyfiles = $$exampleassets.files
}

defineReplace(stripSrcDir) {
    return($$basename(1))
}

assetcopy.input = copyfiles
assetcopy.output = $$OUT_PWD/${QMAKE_FUNC_FILE_IN_stripSrcDir}
assetcopy.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
assetcopy.name = COPY ${QMAKE_FILE_IN}
assetcopy.CONFIG = no_link target_predeps
QMAKE_EXTRA_COMPILERS += assetcopy
