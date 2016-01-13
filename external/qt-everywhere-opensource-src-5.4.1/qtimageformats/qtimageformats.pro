requires(qtHaveModule(gui))

load(configure)
qtCompileTest(jasper)
qtCompileTest(libmng)
qtCompileTest(libtiff)
qtCompileTest(libwebp)

load(qt_parts)
