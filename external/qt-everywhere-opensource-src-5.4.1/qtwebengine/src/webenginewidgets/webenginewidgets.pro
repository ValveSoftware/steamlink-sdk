TARGET = QtWebEngineWidgets

# For our export macros
DEFINES += QT_BUILD_WEBENGINEWIDGETS_LIB

QT += webengine widgets network quick
QT_PRIVATE += webenginecore

QMAKE_DOCS = $$PWD/doc/qtwebenginewidgets.qdocconf

INCLUDEPATH += $$PWD api ../core ../webengine/api

SOURCES = \
        api/qtwebenginewidgetsglobal.cpp \
        api/qwebenginecertificateerror.cpp \
        api/qwebenginehistory.cpp \
        api/qwebenginepage.cpp \
        api/qwebenginesettings.cpp \
        api/qwebengineview.cpp\
        render_widget_host_view_qt_delegate_widget.cpp

HEADERS = \
        api/qtwebenginewidgetsglobal.h \
        api/qwebenginecertificateerror.h \
        api/qwebenginehistory.h \
        api/qwebenginepage.h \
        api/qwebenginepage_p.h \
        api/qwebenginesettings.h \
        api/qwebenginesettings_p.h \
        api/qwebengineview.h \
        api/qwebengineview_p.h \
        render_widget_host_view_qt_delegate_widget.h

load(qt_module)
