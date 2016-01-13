# This .pro file serves a dual purpose:
# 1) invoking gyp through the gyp_qtwebengine script, which in turn makes use of the generated gypi include files
# 2) produce a Makefile that will run ninja, and take care of actually building everything.

TEMPLATE = aux

cross_compile {
    GYP_ARGS = "-D qt_cross_compile=1"
    posix: GYP_ARGS += "-D os_posix=1"
    android: include(config/embedded_android.pri)
    qnx: include(config/embedded_qnx.pri)
    linux:!android: include(config/embedded_linux.pri)
} else {
    # !cross_compile
    GYP_ARGS = "-D qt_cross_compile=0"
    linux: include(config/desktop_linux.pri)
    mac: include(config/mac_osx.pri)
    win32: include(config/windows.pri)
}

GYP_CONFIG += disable_glibcxx_debug=1
!webcore_debug: GYP_CONFIG += remove_webcore_debug_symbols=1

linux:contains(QT_CONFIG, separate_debug_info): GYP_CONFIG += linux_dump_symbols=1

force_debug_info {
    win32: GYP_CONFIG += win_release_extra_cflags=-Zi
    else: GYP_CONFIG += release_extra_cflags=-g
}

# Append additional platform options defined in GYP_CONFIG
for (config, GYP_CONFIG): GYP_ARGS += "-D $$config"

# Copy this logic from qt_module.prf so that ninja can run according
# to the same rules as the final module linking in core_module.pro.
!host_build:if(win32|mac):!macx-xcode {
    contains(QT_CONFIG, debug_and_release):CONFIG += debug_and_release
    contains(QT_CONFIG, build_all):CONFIG += build_all
}

cross_compile {
    TOOLCHAIN_SYSROOT = $$[QT_SYSROOT]

    !isEmpty(TOOLCHAIN_SYSROOT): GYP_ARGS += "-D sysroot=\"$${TOOLCHAIN_SYSROOT}\""

    contains(QT_ARCH, "arm") {
        GYP_ARGS += "-D target_arch=arm"

        # Extract ARM specific compiler options that we have to pass to gyp,
        # but let gyp figure out a default if an option is not present.
        MARCH = $$extractCFlag("-march=.*")
        !isEmpty(MARCH): GYP_ARGS += "-D arm_arch=\"$$MARCH\""

        MTUNE = $$extractCFlag("-mtune=.*")
        GYP_ARGS += "-D arm_tune=\"$$MTUNE\""

        MFLOAT = $$extractCFlag("-mfloat-abi=.*")
        !isEmpty(MFLOAT): GYP_ARGS += "-D arm_float_abi=\"$$MFLOAT\""

        MARMV = $$replace(MARCH, "armv",)
        !isEmpty(MARMV) {
            MARMV = $$split(MARMV,)
            MARMV = $$member(MARMV, 0)
            lessThan(MARMV, 6): error("$$MARCH architecture is not supported")
            GYP_ARGS += "-D arm_version=\"$$MARMV\""
        }

        MFPU = $$extractCFlag("-mfpu=.*")
        !isEmpty(MFPU) {
            # If the toolchain does not explicitly specify to use NEON instructions
            # we use arm_neon_optional for ARMv7 and newer and let chromium decide
            # about the mfpu option.
            contains(MFPU, "neon")|contains(MFPU, "neon-vfpv4"): GYP_ARGS += "-D arm_fpu=\"$$MFPU\" -D arm_neon=1"
            else:!lessThan(MARMV, 7): GYP_ARGS += "-D arm_neon=0 -D arm_neon_optional=1"
            else: GYP_ARGS += "-D arm_fpu=\"$$MFPU\" -D arm_neon=0 -D arm_neon_optional=0"
        }

        contains(QMAKE_CFLAGS, "-mthumb"): GYP_ARGS += "-D arm_thumb=1"
    }

    # Needed for v8, see chromium/v8/build/toolchain.gypi
    GYP_ARGS += "-D CXX=\"$$which($$QMAKE_CXX)\""
}

contains(QT_ARCH, "x86_64"): GYP_ARGS += "-D target_arch=x64"
contains(QT_ARCH, "i386"): GYP_ARGS += "-D target_arch=ia32"

contains(WEBENGINE_CONFIG, proprietary_codecs): GYP_ARGS += "-Dproprietary_codecs=1 -Dffmpeg_branding=Chrome -Duse_system_ffmpeg=0"

!contains(QT_CONFIG, qt_framework): contains(QT_CONFIG, private_tests) {
    GYP_ARGS += "-D qt_install_data=\"$$[QT_INSTALL_DATA]\""
    GYP_ARGS += "-D qt_install_translations=\"$$[QT_INSTALL_TRANSLATIONS]\""
}

!build_pass {
    message("Running gyp_qtwebengine \"$$OUT_PWD\" $${GYP_ARGS}...")
    !system("python $$QTWEBENGINE_ROOT/tools/buildscripts/gyp_qtwebengine \"$$OUT_PWD\" $${GYP_ARGS}"): error("-- running gyp_qtwebengine failed --")
}

build_pass|!debug_and_release {
    ninja.target = invoke_ninja
    ninja.commands = $$findOrBuildNinja() \$\(NINJAFLAGS\) -C "$$OUT_PWD/$$getConfigDir()"
    QMAKE_EXTRA_TARGETS += ninja

    build_pass:build_all: default_target.target = all
    else: default_target.target = first
    default_target.depends = ninja

    QMAKE_EXTRA_TARGETS += default_target
} else {
    # Special GNU make target for the meta Makefile that ensures that our debug and release Makefiles won't both run ninja in parallel.
    notParallel.target = .NOTPARALLEL
    QMAKE_EXTRA_TARGETS += notParallel
}
