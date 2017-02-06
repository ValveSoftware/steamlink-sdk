# Try to reference a variable that does not exist:
MYVAR=$$THIS_VARIABLE_IS_NOT_DEFINED

SOURCES += main.cpp

win32 {
    SOURCES += main_win.cpp
}

unix {
    SOURCES += main_unix.cpp
}

mac {
    SOURCES += main_mac.cpp
}

SOURCES += wildcard/main*.cpp \
#          yadiyada   it should also parse the next line
            wildcard*.cpp


VPATH = vpaths/dependpath

# The purpose of this test is to test expansion of environment variables,
# and to test if the VPATH variable is considered correctly.
if (exists($$member($$(PATH), 0))) {
    SOURCES += main_dependpath.cpp
}

inst.files = qml *.qml
INSTALLS += inst

TRANSLATIONS = project.ts
