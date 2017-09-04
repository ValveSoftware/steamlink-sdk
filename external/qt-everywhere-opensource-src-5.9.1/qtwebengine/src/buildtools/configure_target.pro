# Prevent generating a makefile that attempts to create a lib
TEMPLATE = aux

GN_CPU = $$gnArch($$QT_ARCH)
GN_OS = $$gnOS()

clang: GN_CLANG = true
else: GN_CLANG = false

use_gold_linker: GN_USE_GOLD=true
else: GN_USE_GOLD=false

GN_CONTENTS = \
"gcc_toolchain(\"target\") {" \
"  cc = \"$$which($$QMAKE_CC)\" " \
"  cxx = \"$$which($$QMAKE_CXX)\" " \
"  ld = \"$$which($$QMAKE_LINK)\" " \
"  ar = \"$$which($${CROSS_COMPILE}ar)\" " \
"  nm = \"$$which($${CROSS_COMPILE}nm)\" " \
"  toolchain_args = { " \
"    current_os = \"$$GN_OS\" " \
"    current_cpu = \"$$GN_CPU\" " \
"    is_clang = $$GN_CLANG " \
"    use_gold = $$GN_USE_GOLD " \
"  } " \
"}"

GN_FILE = $$OUT_PWD/../toolchain/BUILD.gn
!build_pass {
    write_file($$GN_FILE, GN_CONTENTS, append)
}

QMAKE_DISTCLEAN += $$GN_FILE
