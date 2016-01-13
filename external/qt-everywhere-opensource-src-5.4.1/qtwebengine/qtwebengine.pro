load(qt_build_config)

# As long as we are a module separate from the rest of Qt, we want to unconditionally build examples.
# Once part of Qt 5, this should be removed and we should respect the Qt wide configuration.
QTWEBENGINE_BUILD_PARTS = $$QT_BUILD_PARTS
QTWEBENGINE_BUILD_PARTS *= examples

load(qt_parts)
