include(common.pri)

gn_args += \
    is_clang=false \
    use_sysroot=false \
    use_kerberos=true \
    enable_notifications=false \
    enable_session_service=false \
    ninja_use_custom_environment_files=false \
    is_multi_dll_chrome=false \
    win_linker_timing=true

isDeveloperBuild() {
    gn_args += \
        is_win_fastlink=true \
        use_incremental_linking=true
} else {
    gn_args += \
        use_incremental_linking=false
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
    force_debug_info: gn_args -= symbol_level=1
    gn_args *= symbol_level=0
}

msvc {
    equals(MSVC_VER, 14.0) {
        MSVS_VERSION = 2015
    } else:equals(MSVC_VER, 15.0) {
        MSVS_VERSION = 2017
    } else {
        fatal("Visual Studio compiler version \"$$MSVC_VER\" is not supported by Qt WebEngine")
    }

    gn_args += visual_studio_version=$$MSVS_VERSION

    SDK_PATH = $$(WINDOWSSDKDIR)
    VS_PATH= $$(VSINSTALLDIR)
    gn_args += visual_studio_path=$$shell_quote($$VS_PATH)
    gn_args += windows_sdk_path=$$shell_quote($$SDK_PATH)

    GN_TARGET_CPU = $$gnArch($$QT_ARCH)
    gn_args += target_cpu=\"$$GN_TARGET_CPU\"

} else {
    fatal("Qt WebEngine for Windows can only be built with the Microsoft Visual Studio C++ compiler")
}
