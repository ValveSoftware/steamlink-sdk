requires(qtHaveModule(quick))
requires(contains(QT_CONFIG, opengl))

qtHaveModule(quick) {
    QT_FOR_CONFIG += quick-private
    requires(qtConfig(quick-shadereffect))
}

load(qt_parts)
