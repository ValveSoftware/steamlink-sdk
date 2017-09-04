# Prevent generating a makefile that attempts to create a lib
TEMPLATE = aux

# Pick up the host toolchain
option(host_build)

GN_HOST_CPU = $$gnArch($$QMAKE_HOST.arch)
GN_TARGET_CPU = $$gnArch($$WEBENGINE_ARCH)
GN_OS = $$gnOS()

clang: GN_CLANG = true
else: GN_CLANG = false

use_gold_linker: GN_USE_GOLD=true
else: GN_USE_GOLD=false

GN_V8_HOST_CPU = $$GN_HOST_CPU
contains(GN_TARGET_CPU, "arm")|contains(GN_TARGET_CPU, "mips")|contains(GN_TARGET_CPU, "x86") {
    # The v8 snapshot need a host that matches bitwidth, so we build makesnapshot to 32-bit variants of host.
    contains(GN_V8_HOST_CPU, x64): GN_V8_HOST_CPU = "x86"
    else: contains(GN_V8_HOST_CPU, arm64): GN_V8_HOST_CPU = "arm"
    else: contains(GN_V8_HOST_CPU, mips64): GN_V8_HOST_CPU = "mips"
}

# We always use the gcc_toolchain, because clang_toolchain is just
# a broken wrapper around it for Google's custom clang binaries.
GN_CONTENTS = \
"import(\"//build/config/sysroot.gni\")" \
"import(\"//build/toolchain/gcc_toolchain.gni\")" \
"gcc_toolchain(\"host\") {" \
"  cc = \"$$which($$QMAKE_CC)\" " \
"  cxx = \"$$which($$QMAKE_CXX)\" " \
"  ld = \"$$which($$QMAKE_LINK)\" " \
"  ar = \"$$which(ar)\" " \
"  nm = \"$$which(nm)\" " \
"  toolchain_args = { " \
"    current_os = \"$$GN_OS\" " \
"    current_cpu = \"$$GN_HOST_CPU\" " \
"    is_clang = $$GN_CLANG " \
"    use_gold = $$GN_USE_GOLD " \
"  } " \
"}" \
"gcc_toolchain(\"v8_snapshot\") {" \
"  cc = \"$$which($$QMAKE_CC)\" " \
"  cxx = \"$$which($$QMAKE_CXX)\" " \
"  ld = \"$$which($$QMAKE_LINK)\" " \
"  ar = \"$$which(ar)\" " \
"  nm = \"$$which(nm)\" " \
"  toolchain_args = { " \
"    current_os = \"$$GN_OS\" " \
"    current_cpu = \"$$GN_V8_HOST_CPU\" " \
"    v8_current_cpu = \"$$GN_TARGET_CPU\" " \
"    is_clang = $$GN_CLANG " \
"    use_gold = $$GN_USE_GOLD " \
"  } " \
" } "

GN_FILE = $$OUT_PWD/../toolchain/BUILD.gn
!build_pass {
    write_file($$GN_FILE, GN_CONTENTS)
}
