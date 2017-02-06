DEFINES += WTF_EXPORT_PRIVATE="" JS_EXPORT_PRIVATE=""
DEFINES += ENABLE_ASSEMBLER_WX_EXCLUSIVE=1

# Avoid symbol clashes with QtScript during static linking
DEFINES += WTFReportAssertionFailure=qmlWTFReportAssertionFailure
DEFINES += WTFReportBacktrace=qmlWTFReportBacktrace
DEFINES += WTFInvokeCrashHook=qmlWTFInvokeCrashHook

win*: DEFINES += NOMINMAX

DEFINES += ENABLE_LLINT=0
DEFINES += ENABLE_DFG_JIT=0
DEFINES += ENABLE_DFG_JIT_UTILITY_METHODS=1
DEFINES += ENABLE_JIT_CONSTANT_BLINDING=0
DEFINES += BUILDING_QT__

INCLUDEPATH += $$PWD/jit
INCLUDEPATH += $$PWD/assembler
INCLUDEPATH += $$PWD/runtime
INCLUDEPATH += $$PWD/wtf
INCLUDEPATH += $$PWD/stubs
INCLUDEPATH += $$PWD/stubs/wtf
INCLUDEPATH += $$PWD

disassembler {
    if(isEqual(QT_ARCH, "i386")|isEqual(QT_ARCH, "x86_64")): DEFINES += WTF_USE_UDIS86=1
    if(isEqual(QT_ARCH, "arm")): DEFINES += WTF_USE_ARMV7_DISASSEMBLER=1
    if(isEqual(QT_ARCH, "arm64")): DEFINES += WTF_USE_ARM64_DISASSEMBLER=1
    if(isEqual(QT_ARCH, "mips")): DEFINES += WTF_USE_MIPS32_DISASSEMBLER=1
} else {
    DEFINES += WTF_USE_UDIS86=0
}

force-compile-jit {
    DEFINES += V4_FORCE_COMPILE_JIT
}

INCLUDEPATH += $$PWD/disassembler
INCLUDEPATH += $$PWD/disassembler/udis86
INCLUDEPATH += $$_OUT_PWD

CONFIG(release, debug|release): DEFINES += NDEBUG
