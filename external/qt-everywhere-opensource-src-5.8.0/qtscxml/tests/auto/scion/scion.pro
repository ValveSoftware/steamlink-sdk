QT = core gui qml testlib scxml
CONFIG += testcase c++11 console
CONFIG -= app_bundle
TARGET = tst_scion

TEMPLATE = app

RESOURCES = $$OUT_PWD/scion.qrc
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

SOURCES += \
    tst_scion.cpp

defineReplace(nameTheNamespace) {
    sn=$$relative_path($$absolute_path($$dirname(1), $$OUT_PWD),$$SCXMLS_DIR)
    sn~=s/\\.txml$//
    sn~=s/[^a-zA-Z_0-9]/_/
    return ($$sn)
}
defineReplace(nameTheClass) {
    cn = $$basename(1)
    cn ~= s/\\.scxml$//
    cn ~=s/\\.txml$//
    cn ~= s/[^a-zA-Z_0-9]/_/
    return ($$cn)
}

qtPrepareTool(QMAKE_QSCXMLC, qscxmlc)

win32 {
    msvc: QMAKE_CXXFLAGS += /bigobj
}

myscxml.commands = $$QMAKE_QSCXMLC --header scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.h --impl ${QMAKE_FILE_OUT} --namespace ${QMAKE_FUNC_nameTheNamespace} --classname ${QMAKE_FUNC_nameTheClass} ${QMAKE_FILE_IN}
myscxml.depends += $$QMAKE_QSCXMLC_EXE
myscxml.output = scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.cpp
myscxml.input = SCXMLS
myscxml.variable_out = SOURCES
QMAKE_EXTRA_COMPILERS += myscxml

myscxml_hdr.input = SCXMLS
myscxml_hdr.variable_out = SCXML_HEADERS
myscxml_hdr.commands = $$escape_expand(\\n)
myscxml_hdr.depends = scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.cpp
myscxml_hdr.output = scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.h
QMAKE_EXTRA_COMPILERS += myscxml_hdr

SCXMLS_DIR += $$absolute_path($$PWD/../../3rdparty/scion-tests/scxml-test-framework/test)
ALLSCXMLS = $$files($$SCXMLS_DIR/*.scxml, true)

# For a better explanation about the "blacklisted" tests, see tst_scion.cpp
# <invoke>
BLACKLISTED = \
    test216sub1.scxml \
    test226sub1.txml \
    test239sub1.scxml \
    test242sub1.scxml \
    test276sub1.scxml \
    test530.txml.scxml

# other
BLACKLISTED += \
    test301.txml.scxml \
    test441a.txml.scxml \
    test441b.txml.scxml \
    test557.txml.scxml

for (f,ALLSCXMLS) {
    cn = $$basename(f)
    if (!contains(BLACKLISTED, $$cn)) {
        SCXMLS += $$f

        cn ~= s/\\.scxml$//
        hn = $$cn
        cn ~=s/\\.txml$//
        sn = $$relative_path($$dirname(f), $$SCXMLS_DIR)
        sn ~=s/[^a-zA-Z_0-9]/_/

        inc_list += "$${LITERAL_HASH}include \"scxml/$${sn}_$${hn}.h\""
        func_list += "    []()->QScxmlStateMachine*{return new $${sn}::$${cn};},"

        base = $$relative_path($$f,$$absolute_path($$SCXMLS_DIR))
        tn = $$base
        tn ~= s/\\.scxml$//
        testBases += "    \"$$tn\","
    }
}

ALLFILES = $$files($$SCXMLS_DIR/*.*, true)
for (f,ALLFILES) {
    base = $$relative_path($$f,$$absolute_path($$SCXMLS_DIR))
    file = $$relative_path($$f, $$absolute_path($$PWD))
    qrc += '<file alias="$$base">$$file</file>'
}

contents = $$inc_list "std::function<QScxmlStateMachine *()> creators[] = {" $$func_list "};"
write_file("scxml/compiled_tests.h", contents)|error("Aborting.")

contents = "const char *testBases[] = {" $$testBases "};"
write_file("scxml/scion.h", contents)|error("Aborting.")

contents = '<!DOCTYPE RCC><RCC version=\"1.0\">' '<qresource>' $$qrc '</qresource></RCC>'
write_file("scion.qrc", contents)|error("Aborting.")
