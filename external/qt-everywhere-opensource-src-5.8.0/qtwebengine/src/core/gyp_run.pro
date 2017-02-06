# This .pro file serves a dual purpose:
# 1) invoking gyp through the gyp_qtwebengine script, which in turn makes use of the generated gypi include files
# 2) produce a Makefile that will run ninja, and take care of actually building everything.

TEMPLATE = aux

cross_compile {
    GYP_ARGS = "-D qt_cross_compile=1"
    posix: GYP_ARGS += "-D os_posix=1"
    qnx: include(config/embedded_qnx.pri)
    linux: include(config/embedded_linux.pri)
} else {
    # !cross_compile
    GYP_ARGS = "-D qt_cross_compile=0"
    linux: include(config/desktop_linux.pri)
    mac: include(config/mac_osx.pri)
    win32: include(config/windows.pri)
}
GYP_CONFIG += qtwe_process_name_debug=$$QTWEBENGINEPROCESS_NAME_DEBUG
GYP_CONFIG += qtwe_process_name_release=$$QTWEBENGINEPROCESS_NAME_RELEASE
GYP_CONFIG += disable_glibcxx_debug=1
!contains(QT_CONFIG, no-pkg-config): GYP_ARGS += "-D pkg-config=\"$$pkgConfigExecutable()\""

!webcore_debug: GYP_CONFIG += remove_webcore_debug_symbols=1
!v8base_debug: GYP_CONFIG += remove_v8base_debug_symbols=1

linux:qtConfig(separate_debug_info): GYP_CONFIG += linux_dump_symbols=1

force_debug_info {
    win32: GYP_CONFIG += win_release_extra_cflags=-Zi
    else: GYP_CONFIG += release_extra_cflags=-g
}

!warnings_are_errors: GYP_CONFIG += disable_fatal_linker_warnings=1

# Copy this logic from qt_module.prf so that ninja can run according
# to the same rules as the final module linking in core_module.pro.
!host_build:if(win32|mac):!macx-xcode {
    qtConfig(debug_and_release): CONFIG += debug_and_release build_all
}

cross_compile {
    TOOLCHAIN_SYSROOT = $$[QT_SYSROOT]
    !isEmpty(TOOLCHAIN_SYSROOT): GYP_CONFIG += sysroot=\"$${TOOLCHAIN_SYSROOT}\"

    # Needed for v8, see chromium/v8/build/toolchain.gypi
    GYP_CONFIG += CXX=\"$$which($$QMAKE_CXX)\"
}
else {
    GYP_CONFIG += sysroot=\"\"
}

contains(QT_ARCH, "arm") {
    GYP_CONFIG += target_arch=arm

    # Extract ARM specific compiler options that we have to pass to gyp,
    # but let gyp figure out a default if an option is not present.
    MARCH = $$extractCFlag("-march=.*")
    !isEmpty(MARCH): GYP_CONFIG += arm_arch=\"$$MARCH\"

    MTUNE = $$extractCFlag("-mtune=.*")
    !isEmpty(MTUNE): GYP_CONFIG += arm_tune=\"$$MTUNE\"

    MFLOAT = $$extractCFlag("-mfloat-abi=.*")
    !isEmpty(MFLOAT): GYP_CONFIG += arm_float_abi=\"$$MFLOAT\"

    MARMV = $$replace(MARCH, "armv",)
    !isEmpty(MARMV) {
        MARMV = $$split(MARMV,)
        MARMV = $$member(MARMV, 0)
        lessThan(MARMV, 6): error("$$MARCH architecture is not supported")
        GYP_CONFIG += arm_version=\"$$MARMV\"
    }

    MFPU = $$extractCFlag("-mfpu=.*")
    !isEmpty(MFPU) {
        # If the toolchain does not explicitly specify to use NEON instructions
        # we use arm_neon_optional for ARMv7 and newer and let chromium decide
        # about the mfpu option.
        contains(MFPU, ".*neon.*"): GYP_CONFIG += arm_fpu=\"$$MFPU\" arm_neon=1
        else:!lessThan(MARMV, 7): GYP_CONFIG += arm_neon=0 arm_neon_optional=1
        else: GYP_CONFIG += arm_fpu=\"$$MFPU\" arm_neon=0 arm_neon_optional=0
    } else {
        # Chromium defaults to arm_neon=1, Qt does not.
        GYP_CONFIG += arm_neon=0
        !lessThan(MARMV, 7): GYP_CONFIG += arm_neon_optional=1
    }

    if(isEmpty(MARMV)|lessThan(MARMV, 7)):contains(QMAKE_CFLAGS, "-marm"): GYP_CONFIG += arm_thumb=0
    contains(QMAKE_CFLAGS, "-mthumb"): GYP_CONFIG += arm_thumb=1
}

contains(QT_ARCH, "mips") {
    GYP_CONFIG += target_arch=mipsel

    MARCH = $$extractCFlag("-march=.*")
    !isEmpty(MARCH) {
        equals(MARCH, "mips32r6"): GYP_CONFIG += mips_arch_variant=\"r6\"
        else: equals(MARCH, "mips32r2"): GYP_CONFIG += mips_arch_variant=\"r2\"
        else: equals(MARCH, "mips32"): GYP_CONFIG += mips_arch_variant=\"r1\"
    } else {
        contains(QMAKE_CFLAGS, "mips32r6"): GYP_CONFIG += mips_arch_variant=\"r6\"
        else: contains(QMAKE_CFLAGS, "mips32r2"): GYP_CONFIG += mips_arch_variant=\"r2\"
        else: contains(QMAKE_CFLAGS, "mips32"): GYP_CONFIG += mips_arch_variant=\"r1\"
    }

    contains(QMAKE_CFLAGS, "-mdsp2"): GYP_CONFIG += mips_dsp_rev=2
    else: contains(QMAKE_CFLAGS, "-mdsp"): GYP_CONFIG += mips_dsp_rev=1
}

contains(QT_ARCH, "x86_64"): GYP_CONFIG += target_arch=x64
contains(QT_ARCH, "i386"): GYP_CONFIG += target_arch=ia32
contains(QT_ARCH, "arm64"): GYP_CONFIG += target_arch=arm64
contains(QT_ARCH, "mips64"): GYP_CONFIG += target_arch=mips64el

contains(WEBENGINE_CONFIG, use_proprietary_codecs): GYP_CONFIG += proprietary_codecs=1 ffmpeg_branding=Chrome
contains(WEBENGINE_CONFIG, use_appstore_compliant_code): GYP_CONFIG += appstore_compliant_code=1

# Compiling with -Os makes a huge difference in binary size, and the unwind tables is another big part,
# but the latter are necessary for useful debug binaries.
contains(WEBENGINE_CONFIG, reduce_binary_size): GYP_CONFIG += release_optimize=s debug_optimize=s release_unwind_tables=0

!contains(WEBENGINE_CONFIG, use_spellchecker): {
    GYP_CONFIG += enable_spellcheck=0
    macos: GYP_CONFIG += use_browser_spellchecker=0
} else {
    GYP_CONFIG += enable_spellcheck=1
    macos {
        contains(WEBENGINE_CONFIG, use_native_spellchecker) {
            GYP_CONFIG += use_browser_spellchecker=1
        } else {
            GYP_CONFIG += use_browser_spellchecker=0
        }
    }
}

!qtConfig(framework):qtConfig(private_tests) {
    GYP_CONFIG += qt_install_data=\"$$[QT_INSTALL_DATA/get]\"
    GYP_CONFIG += qt_install_translations=\"$$[QT_INSTALL_TRANSLATIONS/get]\"
}

# Append additional platform options defined in GYP_CONFIG
for (config, GYP_CONFIG): GYP_ARGS += "-D $$config"

!build_pass {
    message("Running gyp_qtwebengine \"$$OUT_PWD\" $${GYP_ARGS}.")
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
