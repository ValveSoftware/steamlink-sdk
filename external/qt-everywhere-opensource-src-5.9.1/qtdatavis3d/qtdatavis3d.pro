requires(qtHaveModule(widgets))

# We need opengl, minimum es2 or desktop
requires(contains(QT_CONFIG, opengl))
requires(!contains(QT_CONFIG, opengles1))

load(qt_parts)

OTHER_FILES += README dist/* .qmake.conf
