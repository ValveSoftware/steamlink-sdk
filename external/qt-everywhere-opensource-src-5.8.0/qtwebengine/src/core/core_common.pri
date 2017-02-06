# NOTE: The TARGET, QT, QT_PRIVATE variables are used in both core_module.pro and core_gyp_generator.pro
# gyp/ninja will take care of the compilation, qmake/make will finish with linking and install.

TARGET = QtWebEngineCore
QT += qml quick webchannel
QT_PRIVATE += quick-private gui-private core-private webenginecoreheaders-private

# Make QtCreator happy.
CHROMIUM_SRC_DIR = $$QTWEBENGINE_ROOT/$$getChromiumSrcDir()
INCLUDEPATH += $$CHROMIUM_SRC_DIR

qtHaveModule(positioning):QT += positioning
