#
# Automatically detects the T9Write build directory and sets the following variables:
#
#   T9WRITE_BUILD_DIR: A base directory for the architecture specific build directory
#   T9WRITE_ALPHABETIC_OBJ: Absolute path to the target object file
#

contains(QT_ARCH, arm) {
    T9WRITE_BUILD_DIR = $$files(build_Android_ARM*)
} else:linux {
    T9WRITE_BUILD_DIR = $$files(build_Android_x86*)
} else:win32 {
    T9WRITE_BUILD_DIR = $$files(build_VC*)
}

count(T9WRITE_BUILD_DIR, 1) {
    T9WRITE_FOUND = 1
    T9WRITE_INCLUDE_DIRS = \
        $$PWD/$$T9WRITE_BUILD_DIR/api \
        $$PWD/$$T9WRITE_BUILD_DIR/public
    T9WRITE_ALPHABETIC_LIBS = \
        $$PWD/$$files($$T9WRITE_BUILD_DIR/objects/t9write_alphabetic*.o*)
} else {
    T9WRITE_FOUND = 0
}
