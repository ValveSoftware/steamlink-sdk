option(host_build)
TARGET     = QtQmlDevTools
QT         = core-private
CONFIG += static internal_module qmldevtools_build

# Don't use pch because the auto-generated header refers to QtBootstrap,
# which doesn't exist
CONFIG -= precompile_header

# 2415: variable "xx" of static storage duration was declared but never referenced
# unused variable 'xx' [-Werror,-Wunused-const-variable]
intel_icc: WERROR += -ww2415
clang:if(greaterThan(QT_CLANG_MAJOR_VERSION, 3)|greaterThan(QT_CLANG_MINOR_VERSION, 3)): \
    WERROR += -Wno-error=unused-const-variable

include(../3rdparty/masm/masm-defs.pri)
include(../qml/parser/parser.pri)
include(../qml/jsruntime/jsruntime.pri)
include(../qml/compiler/compiler.pri)
include(../qml/memory/memory.pri)

load(qt_module)
