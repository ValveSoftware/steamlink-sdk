GYP_ARGS += "-D qt_os=\"win32\" -I config/windows.gypi"

GYP_CONFIG += \
    disable_nacl=1 \
    remoting=0 \
    use_ash=0 \

# Chromium builds with debug info in release by default but Qt doesn't
CONFIG(release, debug|release):!force_debug_info: GYP_CONFIG += fastbuild=1

# Libvpx build needs additional search path on Windows.
GYP_ARGS += "-D qtwe_chromium_obj_dir=\"$$OUT_PWD/$$getConfigDir()/obj/$${getChromiumSrcDir()}\""

# Use path from environment for perl, bison and gperf instead of values set in WebKit's core.gypi.
GYP_ARGS += "-D perl_exe=\"perl.exe\" -D bison_exe=\"bison.exe\" -D gperf_exe=\"gperf.exe\""

# Gyp's parallel processing is broken on Windows
GYP_ARGS += "--no-parallel"

contains(QT_CONFIG, angle) {
    CONFIG(release, debug|release) {
        GYP_ARGS += "-D qt_egl_library=\"libEGL.lib\" -D qt_glesv2_library=\"libGLESv2.lib\""
    } else {
        GYP_ARGS += "-D qt_egl_library=\"libEGLd.lib\" -D qt_glesv2_library=\"libGLESv2d.lib\""
    }
    GYP_ARGS += "-D qt_gl=\"angle\""
} else {
    GYP_ARGS += "-D qt_gl=\"opengl\""
}

