GYP_ARGS += "-D qt_os=\"win32\" -I config/windows.gypi"

include(common.pri)

GYP_CONFIG += \
    disable_nacl=1 \
    remoting=0 \
    use_ash=0 \
    enable_widevine=1 \
    enable_basic_printing=1 \
    enable_print_preview=1 \
    enable_pdf=1

# Libvpx build needs additional search path on Windows.
GYP_ARGS += "-D qtwe_chromium_obj_dir=\"$$OUT_PWD/$$getConfigDir()/obj/$${getChromiumSrcDir()}\""

# Use path from environment for perl, bison and gperf instead of values set in WebKit's core.gypi.
GYP_ARGS += "-D perl_exe=\"perl.exe\" -D bison_exe=\"bison.exe\" -D gperf_exe=\"gperf.exe\""

# Gyp's parallel processing is broken on Windows
GYP_ARGS += "--no-parallel"

qtConfig(angle) {
    CONFIG(release, debug|release) {
        GYP_ARGS += "-D qt_egl_library=\"libEGL.lib\" -D qt_glesv2_library=\"libGLESv2.lib\""
    } else {
        GYP_ARGS += "-D qt_egl_library=\"libEGLd.lib\" -D qt_glesv2_library=\"libGLESv2d.lib\""
    }
    GYP_ARGS += "-D qt_gl=\"angle\""
} else {
    GYP_ARGS += "-D qt_gl=\"opengl\""
}

defineTest(usingMSVC32BitCrossCompiler) {
    CL_DIR =
    for(dir, QMAKE_PATH_ENV) {
        exists($$dir/cl.exe) {
            CL_DIR = $$dir
            break()
        }
    }
    isEmpty(CL_DIR): {
        warning(Cannot determine location of cl.exe.)
        return(false)
    }
    CL_DIR = $$system_path($$CL_DIR)
    CL_DIR = $$split(CL_DIR, \\)
    CL_PLATFORM = $$last(CL_DIR)
    equals(CL_PLATFORM, amd64_x86): return(true)
    return(false)
}

msvc:contains(QT_ARCH, "i386"):!usingMSVC32BitCrossCompiler() {
    # The 32 bit MSVC linker runs out of memory if we do not remove all debug information.
    GYP_CONFIG += fastbuild=2
} else {
    # Chromium builds with debug info in release by default but Qt doesn't
    CONFIG(release, debug|release):!force_debug_info: GYP_CONFIG += fastbuild=1
}

msvc {
    equals(MSVC_VER, 14.0) {
        MSVS_VERSION = 2015
    } else {
        fatal("Visual Studio compiler version \"$$MSVC_VER\" is not supported by Qt WebEngine")
    }

    GYP_ARGS += "-G msvs_version=$$MSVS_VERSION"

    isBuildingOnWin32(): GYP_ARGS += "-D windows_sdk_path=\"C:/Program Files/Windows Kits/10\""

} else {
    fatal("Qt WebEngine for Windows can only be built with the Microsoft Visual Studio C++ compiler")
}
