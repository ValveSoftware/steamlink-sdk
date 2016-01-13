option(host_build)
TARGET     = QtQmlDevTools
QT         = core
CONFIG += static no_module_headers internal_module qmldevtools_build

MODULE_INCLUDES = \
    \$\$QT_MODULE_INCLUDE_BASE \
    \$\$QT_MODULE_INCLUDE_BASE/QtQml
MODULE_PRIVATE_INCLUDES = \
    \$\$QT_MODULE_INCLUDE_BASE/QtQml/$$QT.qml.VERSION \
    \$\$QT_MODULE_INCLUDE_BASE/QtQml/$$QT.qml.VERSION/QtQml

# 2415: variable "xx" of static storage duration was declared but never referenced
# unused variable 'xx' [-Werror,-Wunused-const-variable]
intel_icc: WERROR += -ww2415
clang:if(greaterThan(QT_CLANG_MAJOR_VERSION, 3)|greaterThan(QT_CLANG_MINOR_VERSION, 3)): \
    WERROR += -Wno-error=unused-const-variable

load(qt_module)

include(../3rdparty/masm/masm-defs.pri)
include(../qml/parser/parser.pri)
include(../qml/jsruntime/jsruntime.pri)
include(../qml/compiler/compiler.pri)
include(../qml/qml/qml.pri)
