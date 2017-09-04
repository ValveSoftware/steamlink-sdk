equals(QMAKE_HOST.os, Windows): EXE_SUFFIX = .exe

defineTest(isPythonVersionSupported) {
    python = $$system_quote($$system_path($$1))
    python_version = $$system('$$python -c "import sys; print(sys.version_info[0:3])"')
    python_version ~= s/[()]//g
    python_version = $$split(python_version, ',')
    python_major_version = $$first(python_version)
    greaterThan(python_major_version, 2) {
        qtLog("Python version 3 is not supported by Chromium.")
        return(false)
    }
    python_minor_version = $$member(python_version, 1)
    python_patch_version = $$member(python_version, 2)
    greaterThan(python_major_version, 1): greaterThan(python_minor_version, 6): greaterThan(python_patch_version, 4): return(true)
    qtLog("Unsupported python version: $${python_major_version}.$${python_minor_version}.$${python_patch_version}.")
    return(false)
}

defineTest(qtConfTest_detectPython2) {
    python = $$qtConfFindInPath("python2")
    isEmpty(python) {
        qtLog("'python2' not found in PATH. Checking for 'python'.")
        python = $$qtConfFindInPath("python$$EXE_SUFFIX")
    }
    isEmpty(python) {
        qtLog("'python$$EXE_SUFFIX' not found in PATH. Giving up.")
        return(false)
    }
    !isPythonVersionSupported($$python) {
        qtLog("A suitable Python 2 executable could not be located.")
        return(false)
    }

    # Make tests.python2.location available in configure.json.
    $${1}.location = $$clean_path($$python)
    export($${1}.location)
    $${1}.cache += location
    export($${1}.cache)

    return(true)
}

defineTest(qtConfTest_detectNinja) {
    ninja = $$qtConfFindInPath("ninja$$EXE_SUFFIX")
    !isEmpty(ninja) {
        qtLog("Found ninja from path: $$ninja")
        qtRunLoggedCommand("$$ninja --version", version)|return(false)
        contains(version, "1.*"): return(true)
        qtLog("Ninja version too old")
    }
    qtLog("Building own ninja")
    return(false)
}

defineTest(qtConfTest_detectGn) {
    gn = $$qtConfFindInPath("gn$$EXE_SUFFIX")
    !isEmpty(gn) {
        qtRunLoggedCommand("$$gn --version", version)|return(false)
        #accept all for now
        contains(version, ".*"): return(true)
        qtLog("Gn version too old")
    }
    qtLog("Building own gn")
    return(false)
}

defineTest(qtConfTest_embedded) {
    lessThan(QT_MINOR_VERSION, 9) {
        cross_compile: return(true)
        return(false)
    }
    $$qtConfEvaluate("features.cross_compile"): return(true)
    return(false)
}
