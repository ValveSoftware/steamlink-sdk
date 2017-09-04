macos {
    CONFIG -= app_bundle

    # QTBUG-57354 embed Info.plist so that certain fonts can be found in non-bundle apps
    out_info = $$OUT_PWD/Info.plist
    embed_info_plist.input = $$PWD/Info.plist.in
    embed_info_plist.output = $$out_info
    TARGET_HYPHENATED = $$replace(TARGET, [^a-zA-Z0-9-.], -)
    QMAKE_SUBSTITUTES += embed_info_plist
    QMAKE_LFLAGS += -Wl,-sectcreate,__TEXT,__info_plist,$$shell_quote($$out_info)
    PRE_TARGETDEPS += $$out_info
    QMAKE_DISTCLEAN += $$out_info
}
