TEMPLATE = aux

load(qt_build_paths)

qtPrepareTool(LRELEASE, lrelease)
qtPrepareTool(LCONVERT, lconvert)
qtPrepareTool(LUPDATE, lupdate)
LUPDATE += -locations relative -no-ui-lines

TS_TARGETS =

# meta target name, target name, project files, ts files
defineTest(addTsTarget) {
    dir = $$section(PWD, /, 0, -3)
    for(p, 3): \
        exists($$dir/$$p): \
            pros += -pro-out $$shell_quote($$shadowed($$dir/$$dirname(p))) $$p
    isEmpty(pros): return()
    cv = $${2}.commands
    $$cv = cd $$dir && $$LUPDATE $$pros -ts $$4
    export($$cv)
    dv = $${1}.depends
    $$dv += $$2
    export($$dv)
    TS_TARGETS += $$1 $$2
    export(TS_TARGETS)
}

TS_MODULES =

# target basename, project files
defineTest(addTsTargets) {
    files = $$files($$PWD/$${1}_??.ts) $$files($$PWD/$${1}_??_??.ts)
    for(file, files) {
        lang = $$replace(file, .*_((.._)?..)\\.ts$, \\1)
        addTsTarget(ts-$$lang, ts-$$1-$$lang, $$2, $$file)
    }
    addTsTarget(ts-untranslated, ts-$$1-untranslated, $$2, $$PWD/$${1}_untranslated.ts)
    addTsTarget(ts-all, ts-$$1-all, $$2, $$PWD/$${1}_untranslated.ts $$files)
    TS_MODULES += $$1
    export(TS_MODULES)
}

addTsTargets(qtbase, qtbase/src/src.pro \
    qtactiveqt/src/src.pro \  # just 4 strings from QAxSelect
    qtimageformats/src/src.pro \  # just 10 error messages from tga reader. uses bad context.
)
addTsTargets(qtdeclarative, qtdeclarative/src/src.pro)
addTsTargets(qtquickcontrols, qtquickcontrols/src/src.pro)
addTsTargets(qtquickcontrols2, qtquickcontrols2/src/src.pro)
addTsTargets(qtmultimedia, qtmultimedia/src/src.pro)
addTsTargets(qtquick1, qtquick1/src/src.pro)
addTsTargets(qtscript, qtscript/src/src.pro)
#addTsTargets(qtsvg, qtsvg/src/src.pro) # empty
addTsTargets(qtxmlpatterns, qtxmlpatterns/src/src.pro)
#addTsTargets(qtwebkit, qtwebkit/WebKit.pro) # messages from test browser only

#addTsTargets(qt3d, qt3d/src/src.pro)  # empty except one dubious error message
addTsTargets(qtconnectivity, qtconnectivity/src/src.pro)
#addTsTargets(qtdocgallery, qtdocgallery/src/src.pro)  # dead module
#addTsTargets(qtfeedback, qtfeedback/src/src.pro)  # empty
#addTsTargets(qtjsondb, qtjsondb/src/src.pro)  # dead module, just 3 error messages
addTsTargets(qtlocation, qtlocation/src/src.pro)
#addTsTargets(qtpim, qtpim/src/src.pro)  # not part of 5.0
#addTsTargets(qtsensors, qtsensors/src/src.pro) # empty
#addTsTargets(qtsystems, qtsystems/src/src.pro)  # not part of 5.0
addTsTargets(qtwebsockets, qtwebsockets/src/src.pro)
addTsTargets(qtserialport, qtserialport/src/src.pro)
addTsTargets(qtwebengine, qtwebengine/src/src.pro)

addTsTargets(designer, qttools/src/designer/designer.pro)
addTsTargets(linguist, qttools/src/linguist/linguist/linguist.pro)
addTsTargets(assistant, qttools/src/assistant/assistant/assistant.pro)  # add qcollectiongenerator here as well?
addTsTargets(qt_help, qttools/src/assistant/help/help.pro)
#addTsTargets(qtconfig, qttools/src/qtconfig/qtconfig.pro)  # dead tool
addTsTargets(qmlviewer, qtquick1/tools/qml/qml.pro)
#addTsTargets(qmlscene, qtdeclarative/tools/qmlscene/qmlscene.pro)  # almost empty due to missing tr()

check-ts.commands = (cd $$PWD && perl check-ts.pl)
check-ts.depends = ts-all

isEqual(QMAKE_DIR_SEP, /) {
    commit-ts.commands = \
        cd $$PWD/..; \
        git add -N \"translations/*_??.ts\" && \
        for f in `git diff-files --name-only translations/*_??.ts`; do \
            $$LCONVERT -locations none -i \$\$f -o \$\$f; \
        done; \
        git add \"translations/*_??.ts\" && git commit
} else {
    wd = $$replace(PWD, /, \\)\\..
    commit-ts.commands = \
        cd $$wd && \
        git add -N \"translations/*_??.ts\" && \
        for /f usebackq %%f in (`git diff-files --name-only translations/*_??.ts`) do \
            $$LCONVERT -locations none -i %%f -o %%f $$escape_expand(\\n\\t) \
        cd $$wd && git add \"translations/*_??.ts\" && git commit
}

ts.commands = \
    @echo \"The \'ts\' target has been removed in favor of more fine-grained targets.\" && \
    echo \"Use \'ts-<target>-<lang>\' or \'ts-<lang>\' instead. To add a language,\" && \
    echo \"use \'untranslated\' for <lang>, rename the files and re-run \'qmake\'.\"

QMAKE_EXTRA_TARGETS += $$unique(TS_TARGETS) ts commit-ts check-ts

updateqm.input = TRANSLATIONS
updateqm.output = $$MODULE_BASE_OUTDIR/translations/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
silent:updateqm.commands = @echo lrelease ${QMAKE_FILE_IN} && $$updateqm.commands
updateqm.depends = $$LRELEASE_EXE
updateqm.name = LRELEASE ${QMAKE_FILE_IN}
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm

# generate empty _en.ts files
empty_ts = "<TS></TS>"
for (module_name, TS_MODULES) {
    write_file($$OUT_PWD/$${module_name}_en.ts, empty_ts)|error("Aborting.")
}
write_file($$OUT_PWD/qt_en.ts, empty_ts)|error("Aborting.")

TRANSLATIONS = $$files(*.ts)
!isEqual(OUT_PWD, $$PWD): TRANSLATIONS += $$files($$OUT_PWD/*.ts)

translations.path = $$[QT_INSTALL_TRANSLATIONS]
translations.files = $$TRANSLATIONS
translations.files ~= s,\\.ts$,.qm,g
translations.files ~= s,^$$re_escape($$OUT_PWD),,g
translations.files ~= s,^,$$MODULE_BASE_OUTDIR/translations/,g
translations.CONFIG += no_check_exist
INSTALLS += translations
